#include "shadowpass.hpp"
#include "helpers.hpp"
#include "images.hpp"
#include "initializers.hpp"

auto ShadowPassArray::create(
    VkDevice const device,
    DescriptorAllocator &descriptorAllocator,
    VmaAllocator const allocator,
    VkExtent3D const shadowmapExtent,
    size_t const capacity
) -> std::optional<ShadowPassArray>
{
    VkSamplerCreateInfo const samplerInfo{
        vkinit::samplerCreateInfo(
            0
            , VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK
            , VK_FILTER_NEAREST
            , VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
        )
    };

    ShadowPassArray shadowPass{};
    shadowPass.m_allocator = allocator;

    { // sampler
        VkResult const samplerResult{
            vkCreateSampler(
                device
                , &samplerInfo
                , nullptr
                , &shadowPass.m_sampler
            )
        };

        if (samplerResult != VK_SUCCESS)
        {
            LogVkResult(samplerResult, "Creating Shadow Pass Sampler");
            return {};
        }

        std::vector<VkSampler> const immutableSamplers{
            1, shadowPass.m_sampler
        };

        std::optional<VkDescriptorSetLayout> buildResult{
            DescriptorLayoutBuilder{}
            .addBinding(
                DescriptorLayoutBuilder::AddBindingParameters{
                    .binding = 0,
                    .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .stageMask = VK_SHADER_STAGE_FRAGMENT_BIT
                        | VK_SHADER_STAGE_COMPUTE_BIT,
                    .bindingFlags = 0,
                }
                , immutableSamplers
            )
            .build(device, 0)
        };
        if (!buildResult.has_value())
        {
            Warning(
                "Unable to build ShadowPassArray sampler descriptor layout."
            );
            return {};
        }

        shadowPass.m_samplerSetLayout = buildResult.value();

        shadowPass.m_samplerSet = descriptorAllocator.allocate(
            device
            , shadowPass.m_samplerSetLayout
        );

        // No need to write into this set since we use an immutable sampler.
    }

    { // shadow map textures
        for (size_t i{ 0 }; i < capacity; i++)
        {
            std::optional<AllocatedImage> imageResult{
                AllocatedImage::allocate(
                    allocator
                    , device
                    , AllocatedImage::AllocationParameters{
                        .extent = shadowmapExtent,
                        .format = VK_FORMAT_D32_SFLOAT,
                        .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        .viewFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                    }
                )
            };
            if (!imageResult.has_value())
            {
                Warning("Unable to allocate ShadowPassArray texture.");
                return {};
            }

            shadowPass.m_textures.push_back(imageResult.value());
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
                    .bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                }
                , capacity
            )
            .build(device, 0)
        };
        if (!buildResult.has_value())
        {
            Warning(
                "Unable to build ShadowPassArray textures descriptor layout."
            );
            return {};
        }

        shadowPass.m_texturesSetLayout = buildResult.value();

        shadowPass.m_texturesSet = descriptorAllocator.allocate(
            device
            , shadowPass.m_texturesSetLayout
        );

        std::vector<VkDescriptorImageInfo> mapInfos{};
        for (AllocatedImage const& texture : shadowPass.m_textures)
        {
            mapInfos.push_back(VkDescriptorImageInfo{
                .sampler = VK_NULL_HANDLE, // sampled images
                .imageView = texture.imageView,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
            });
        }

        VkWriteDescriptorSet const shadowMapWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = shadowPass.m_texturesSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(mapInfos.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,

            .pImageInfo = mapInfos.data(),
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        std::vector<VkWriteDescriptorSet> const writes{
            shadowMapWrite
        };
        vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
    }

    shadowPass.m_projViewMatrices = std::make_unique<TStagedBuffer<glm::mat4x4>>(
        TStagedBuffer<glm::mat4x4>::allocate(device, allocator, SHADOWPASS_CAMERA_CAPACITY, 0)
    );
    shadowPass.m_pipeline = std::make_unique<OffscreenPassGraphicsPipeline>(
        device
        , VK_FORMAT_D32_SFLOAT
    );

    return shadowPass;
}

void ShadowPassArray::recordInitialize(
    VkCommandBuffer const cmd
    , ShadowPassParameters parameters
    , std::span<gputypes::LightDirectional const> const directionalLights
    , std::span<gputypes::LightSpot const> const spotLights
)
{
    m_depthBias = parameters.depthBiasConstant;
    m_depthBiasSlope = parameters.depthBiasSlope;

    { 
        // Copy the projection * view matrices that give 
        // the light's POV for each shadow map
        
        TStagedBuffer<glm::mat4x4>& projViewMatrices{ *m_projViewMatrices };
        projViewMatrices.clearStaged();

        size_t shadowMapCount{ 0 };
        for (gputypes::LightDirectional const& light : directionalLights)
        {
            projViewMatrices.push(light.projection * light.view);

            shadowMapCount += 1;
        }
        for (gputypes::LightSpot const& light : spotLights)
        {
            projViewMatrices.push(light.projection * light.view);

            shadowMapCount += 1;
        }

        if (projViewMatrices.stagedSize() > m_textures.size())
        {
            Warning("Not enough shadow maps allocated, skipping work.");
            projViewMatrices.pop(
                projViewMatrices.stagedSize() 
                - m_textures.size()
            );
        }

        projViewMatrices.recordCopyToDevice(cmd, m_allocator);
        projViewMatrices.recordTotalCopyBarrier(
            cmd
            , VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
            , VK_ACCESS_2_SHADER_READ_BIT
        );
    }

    { // Clear each shadow map we are going to use
        m_texturesCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        recordTransitionActiveShadowMaps(cmd, VK_IMAGE_LAYOUT_GENERAL);

        for (size_t i{ 0 }; i < m_projViewMatrices->deviceSize(); i++)
        {
            AllocatedImage& texture{ m_textures[i] };

            VkClearDepthStencilValue const clearValue{
                .depth = 0.0,
            };

            VkImageSubresourceRange const range{
                vkinit::imageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)
            };

            vkCmdClearDepthStencilImage(
                cmd
                , texture.image
                , VK_IMAGE_LAYOUT_GENERAL
                , &clearValue
                , 1, &range
            );
        }
        
        // Prepare for recording of draw commands
        recordTransitionActiveShadowMaps(
            cmd
            , VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
        );
    }
}

void ShadowPassArray::recordDrawCommands(
    VkCommandBuffer const cmd
    , MeshAsset const& mesh
    , TStagedBuffer<glm::mat4x4> const& models
)
{
    for (size_t i{ 0 }; i < m_projViewMatrices->deviceSize(); i++)
    {
        AllocatedImage& texture{ m_textures[i] };
        m_pipeline->recordDrawCommands(
            cmd
            , false
            , m_depthBias
            , m_depthBiasSlope
            , texture
            , i
            , *m_projViewMatrices
            , mesh
            , models
        );
    }
}

void ShadowPassArray::recordTransitionActiveShadowMaps(
    VkCommandBuffer const cmd
    , VkImageLayout const dstLayout
)
{
    for (size_t i{ 0 }; i < m_projViewMatrices->deviceSize(); i++)
    {
        AllocatedImage& texture{ m_textures[i] };
        vkutil::transitionImage(
            cmd
            , texture.image
            , m_texturesCurrentLayout
            , dstLayout
            , VK_IMAGE_ASPECT_DEPTH_BIT
        );
    }

    m_texturesCurrentLayout = dstLayout;
}
