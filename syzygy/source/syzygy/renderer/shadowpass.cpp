#include "shadowpass.hpp"

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include "syzygy/renderer/rendercommands.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include <utility>

namespace syzygy
{
struct MeshInstanced;
} // namespace syzygy

namespace syzygy
{
auto ShadowPassArray::create(
    VkDevice const device,
    DescriptorAllocator& descriptorAllocator,
    VmaAllocator const allocator,
    VkExtent2D const shadowmapExtent,
    size_t const capacity
) -> std::optional<ShadowPassArray>
{
    VkSamplerCreateInfo const samplerInfo{samplerCreateInfo(
        0,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_FILTER_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
    )};

    ShadowPassArray shadowPass{};
    shadowPass.m_allocator = allocator;

    { // sampler
        VkResult const samplerResult{vkCreateSampler(
            device, &samplerInfo, nullptr, &shadowPass.m_sampler
        )};

        if (samplerResult != VK_SUCCESS)
        {
            SZG_LOG_VK(samplerResult, "Creating Shadow Pass Sampler");
            return {};
        }

        std::vector<VkSampler> const immutableSamplers{1, shadowPass.m_sampler};

        std::optional<VkDescriptorSetLayout> buildResult{
            DescriptorLayoutBuilder{}
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_FRAGMENT_BIT
                                   | VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = 0,
                    },
                    immutableSamplers
                )
                .build(device, 0)
        };
        if (!buildResult.has_value())
        {
            SZG_WARNING(
                "Unable to build ShadowPassArray sampler descriptor layout."
            );
            return {};
        }

        shadowPass.m_samplerSetLayout = buildResult.value();

        shadowPass.m_samplerSet =
            descriptorAllocator.allocate(device, shadowPass.m_samplerSetLayout);

        // No need to write into this set since we use an immutable sampler.
    }

    { // shadow map textures
        for (size_t i{0}; i < capacity; i++)
        {
            std::optional<std::unique_ptr<ImageView>> imageResult{
                ImageView::allocate(
                    device,
                    allocator,
                    ImageAllocationParameters{
                        .extent = shadowmapExtent,
                        .format = VK_FORMAT_D32_SFLOAT,
                        .usageFlags =
                            VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    },
                    ImageViewAllocationParameters{
                        .subresourceRange =
                            imageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT),
                    }
                )
            };
            if (!imageResult.has_value())
            {
                SZG_WARNING("Unable to allocate ShadowPassArray texture.");
                return {};
            }

            shadowPass.m_shadowmaps.push_back(std::move(imageResult).value());
        }
    }

    { // textures descriptors
        std::optional<VkDescriptorSetLayout> buildResult{
            DescriptorLayoutBuilder{}
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        .stageMask = VK_SHADER_STAGE_FRAGMENT_BIT
                                   | VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags =
                            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                    },
                    capacity
                )
                .build(device, 0)
        };
        if (!buildResult.has_value())
        {
            SZG_WARNING(
                "Unable to build ShadowPassArray textures descriptor layout."
            );
            return {};
        }

        shadowPass.m_shadowmapSetLayout = buildResult.value();

        shadowPass.m_shadowmapSet = descriptorAllocator.allocate(
            device, shadowPass.m_shadowmapSetLayout
        );

        std::vector<VkDescriptorImageInfo> mapInfos{};
        mapInfos.reserve(shadowPass.m_shadowmaps.size());
        for (std::unique_ptr<ImageView> const& texture :
             shadowPass.m_shadowmaps)
        {
            mapInfos.push_back(VkDescriptorImageInfo{
                .sampler = VK_NULL_HANDLE, // sampled images
                .imageView = texture->view(),
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
            });
        }

        VkWriteDescriptorSet const shadowMapWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = shadowPass.m_shadowmapSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(mapInfos.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,

            .pImageInfo = mapInfos.data(),
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        std::vector<VkWriteDescriptorSet> const writes{shadowMapWrite};
        vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
    }

    shadowPass.m_projViewMatrices =
        std::make_unique<TStagedBuffer<glm::mat4x4>>(
            TStagedBuffer<glm::mat4x4>::allocate(
                device, allocator, SHADOWPASS_CAMERA_CAPACITY, 0
            )
        );
    shadowPass.m_pipeline = std::make_unique<OffscreenPassGraphicsPipeline>(
        device, VK_FORMAT_D32_SFLOAT
    );

    return shadowPass;
}

void ShadowPassArray::recordInitialize(
    VkCommandBuffer const cmd,
    ShadowPassParameters parameters,
    std::span<DirectionalLightPacked const> const directionalLights,
    std::span<SpotLightPacked const> const spotLights
)
{
    m_depthBias = parameters.depthBiasConstant;
    m_depthBiasSlope = parameters.depthBiasSlope;

    {
        // Copy the projection * view matrices that give
        // the light's POV for each shadow map

        TStagedBuffer<glm::mat4x4>& projViewMatrices{*m_projViewMatrices};
        projViewMatrices.clearStaged();

        size_t shadowMapCount{0};
        for (DirectionalLightPacked const& light : directionalLights)
        {
            projViewMatrices.push(light.projection * light.view);

            shadowMapCount += 1;
        }
        for (SpotLightPacked const& light : spotLights)
        {
            projViewMatrices.push(light.projection * light.view);

            shadowMapCount += 1;
        }

        if (projViewMatrices.stagedSize() > m_shadowmaps.size())
        {
            SZG_WARNING("Not enough shadow maps allocated, skipping work.");
            projViewMatrices.pop(
                projViewMatrices.stagedSize() - m_shadowmaps.size()
            );
        }

        projViewMatrices.recordCopyToDevice(cmd);
        projViewMatrices.recordTotalCopyBarrier(
            cmd,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT
        );
    }

    { // Clear each shadow map we are going to use
        for (size_t i{0}; i < m_projViewMatrices->deviceSize(); i++)
        {
            recordClearDepthImage(
                cmd, m_shadowmaps[i]->image(), DEPTH_FAR_STENCIL_NONE
            );
        }

        // Prepare for recording of draw commands
        recordTransitionActiveShadowMaps(
            cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
        );
    }
}

void ShadowPassArray::recordDrawCommands(
    VkCommandBuffer const cmd,
    std::span<MeshInstanced const> const geometry,
    std::span<RenderOverride const> const renderOverrides
)
{
    for (size_t i{0}; i < m_projViewMatrices->deviceSize(); i++)
    {
        m_pipeline->recordDrawCommands(
            cmd,
            false,
            m_depthBias,
            m_depthBiasSlope,
            *m_shadowmaps[i],
            i,
            *m_projViewMatrices,
            geometry,
            renderOverrides
        );
    }
}

void ShadowPassArray::recordTransitionActiveShadowMaps(
    VkCommandBuffer const cmd, VkImageLayout const dstLayout
)
{
    for (size_t i{0}; i < m_projViewMatrices->deviceSize(); i++)
    {
        m_shadowmaps[i]->recordTransitionBarriered(cmd, dstLayout);
    }
}

auto ShadowPassArray::samplerSetLayout() const -> VkDescriptorSetLayout
{
    return m_samplerSetLayout;
}

auto ShadowPassArray::texturesSetLayout() const -> VkDescriptorSetLayout
{
    return m_shadowmapSetLayout;
}

auto ShadowPassArray::samplerSet() const -> VkDescriptorSet
{
    return m_samplerSet;
}

auto ShadowPassArray::textureSet() const -> VkDescriptorSet
{
    return m_shadowmapSet;
}

void ShadowPassArray::cleanup(
    VkDevice const device, VmaAllocator const /*allocator*/
)
{
    for (auto& image : m_shadowmaps)
    {
        image.reset();
    }

    vkDestroySampler(device, m_sampler, nullptr);

    if (m_pipeline)
    {
        m_pipeline->cleanup(device);
    }

    vkDestroyDescriptorSetLayout(device, m_samplerSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_shadowmapSetLayout, nullptr);

    m_projViewMatrices.reset();

    m_shadowmaps.clear();
    m_pipeline.reset();
    m_sampler = VK_NULL_HANDLE;
    m_samplerSetLayout = VK_NULL_HANDLE;
    m_samplerSet = VK_NULL_HANDLE;
    m_shadowmapSetLayout = VK_NULL_HANDLE;
    m_shadowmapSet = VK_NULL_HANDLE;
}
} // namespace syzygy