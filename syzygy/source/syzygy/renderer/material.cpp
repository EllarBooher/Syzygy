#include "material.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include <array>
#include <utility>

syzygy::MaterialDescriptors::MaterialDescriptors(MaterialDescriptors&& other
) noexcept
{
    destroy();

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
    m_colorLayout = std::exchange(other.m_colorLayout, VK_NULL_HANDLE);
    m_colorSet = std::exchange(other.m_colorSet, VK_NULL_HANDLE);
}

syzygy::MaterialDescriptors::~MaterialDescriptors() { destroy(); }

void syzygy::MaterialDescriptors::destroy()
{
    if (m_colorLayout != VK_NULL_HANDLE)
    {
        if (m_device == VK_NULL_HANDLE)
        {
            SZG_WARNING("Device was null when destroying MaterialDescriptors "
                        "with non-null color layout.");
        }
        else
        {
            vkDestroyDescriptorSetLayout(m_device, m_colorLayout, nullptr);
        }
    }
    m_colorLayout = VK_NULL_HANDLE;

    if (m_sampler != VK_NULL_HANDLE)
    {
        if (m_device == VK_NULL_HANDLE)
        {
            SZG_WARNING("Device was null when destroying MaterialDescriptors "
                        "with non-null sampler.");
        }
        else
        {
            vkDestroySampler(m_device, m_sampler, nullptr);
        }
    }

    m_colorSet = VK_NULL_HANDLE;

    m_device = VK_NULL_HANDLE;
}

auto syzygy::MaterialDescriptors::create(
    VkDevice const device, DescriptorAllocator& descriptorAllocator
) -> std::optional<MaterialDescriptors>
{
    std::optional<MaterialDescriptors> descriptorsResult{MaterialDescriptors{}};
    MaterialDescriptors& descriptors{descriptorsResult.value()};

    descriptors.m_device = device;

    if (std::optional<VkDescriptorSetLayout> colorLayoutResult{
            DescriptorLayoutBuilder{} // Color
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    1
                )
                // Normal
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 1,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    1
                )
                // ORM
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 2,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    1
                )
                .build(descriptors.m_device, static_cast<VkFlags>(0))
        };
        colorLayoutResult.has_value())
    {
        descriptors.m_colorLayout = colorLayoutResult.value();
    }
    else
    {
        SZG_ERROR(
            "Unable to allocate Descriptor Set Layout for Material's Color set."
        );
        return std::nullopt;
    }

    {
        VkSamplerCreateInfo const samplerInfo{samplerCreateInfo(
            static_cast<VkFlags>(0),
            VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT
        )};

        SZG_TRY_VK(
            vkCreateSampler(
                device, &samplerInfo, nullptr, &descriptors.m_sampler
            ),
            "Unable to create sampler for Material's descriptors.",
            std::nullopt
        );
    }

    descriptors.m_colorSet = descriptorAllocator.allocate(
        descriptors.m_device, descriptors.m_colorLayout
    );

    return descriptorsResult;
}

void syzygy::MaterialDescriptors::write(MaterialData const& material) const
{
    if (material.color != nullptr)
    {
        VkDescriptorImageInfo const colorImageInfo{
            .sampler = m_sampler,
            .imageView = material.color->view(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        VkDescriptorImageInfo const normalMapInfo{
            .sampler = m_sampler,
            .imageView = material.normal->view(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        VkDescriptorImageInfo const ormMapInfo{
            .sampler = m_sampler,
            .imageView = material.ORM->view(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        std::array<VkDescriptorImageInfo, 3> imageInfos{
            colorImageInfo, normalMapInfo, ormMapInfo
        };

        VkWriteDescriptorSet const writeInfo{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_colorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = imageInfos.size(),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = imageInfos.data(),
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        vkUpdateDescriptorSets(m_device, 1, &writeInfo, 0, nullptr);
    }
}

void syzygy::MaterialDescriptors::bind(
    VkCommandBuffer const cmd,
    VkPipelineLayout const pipelineLayout,
    uint32_t const colorSet
) const
{
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        colorSet,
        1,
        &m_colorSet,
        0,
        nullptr
    );
}
