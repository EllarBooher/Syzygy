#include "skyview.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace syzygy
{
struct CameraPacked;
} // namespace syzygy

namespace detail
{
auto createLayout(
    VkDevice const device,
    std::span<VkDescriptorSetLayout const> const setLayouts,
    std::span<VkPushConstantRange const> const ranges
) -> VkPipelineLayout
{
    VkPipelineLayoutCreateInfo const layoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,

        .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
        .pSetLayouts = setLayouts.data(),

        .pushConstantRangeCount = static_cast<uint32_t>(ranges.size()),
        .pPushConstantRanges = ranges.data(),
    };

    VkPipelineLayout layout{VK_NULL_HANDLE};
    VkResult const result{
        vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &layout)
    };
    if (result != VK_SUCCESS)
    {
        SZG_LOG_VK(result, "Creating shader object pipeline layout");
        return VK_NULL_HANDLE;
    }
    return layout;
}
auto computeDispatchCount(uint32_t invocations, uint32_t workgroupSize)
    -> uint32_t
{
    // When workgroups are larger than 1, but this value does not evenly divide
    // the amount of work needed, we need to dispatch extra to cover this. It is
    // up to the shader to discard these extra invocations.

    uint32_t const count{invocations / workgroupSize};

    if (invocations % workgroupSize == 0)
    {
        return count;
    }

    return count + 1;
}
auto populateTransmittanceResources(
    VkDevice const device,
    VmaAllocator const allocator,
    syzygy::DescriptorAllocator& descriptorAllocator,
    syzygy::SkyViewComputePipeline::TransmittanceLUTResources& resources
) -> bool
{
    VkExtent2D constexpr EXTENT_LUT{1024U, 256U};

    if (auto mapResult{syzygy::ImageView::allocate(
            device,
            allocator,
            syzygy::ImageAllocationParameters{
                .extent = EXTENT_LUT,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .usageFlags =
                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            },
            syzygy::ImageViewAllocationParameters{}
        )};
        mapResult.has_value())
    {
        resources.map = std::move(mapResult).value();
    }
    else
    {
        SZG_ERROR("Failed to allocate transmittance LUT map.");
        return false;
    }

    if (auto setLayoutResult{
            syzygy::DescriptorLayoutBuilder{}
                .addBinding(
                    syzygy::DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    1
                )
                .build(device, static_cast<VkFlags>(0))
        };
        setLayoutResult.has_value())
    {
        resources.setLayout = setLayoutResult.value();
    }
    else
    {
        SZG_ERROR(
            "Failed to allocate transmittance LUT descriptor set 0 layout."
        );
        return false;
    }

    resources.set = descriptorAllocator.allocate(device, resources.setLayout);

    std::array<VkDescriptorSetLayout, 1> const setLayouts{resources.setLayout};
    if (auto const& layoutResult{detail::createLayout(device, setLayouts, {})};
        layoutResult != VK_NULL_HANDLE)
    {
        resources.layout = layoutResult;
    }
    else
    {
        SZG_ERROR("Failed to allocate transmittance LUT pipeline layout.");
        return false;
    }

    if (auto shaderResult{syzygy::loadShaderObject(
            device,
            "shaders/atmosphere/transmittance_LUT.comp.spv",
            VK_SHADER_STAGE_COMPUTE_BIT,
            static_cast<VkFlags>(0),
            setLayouts,
            {}
        )};
        shaderResult.has_value())
    {
        resources.shader = shaderResult.value();
    }
    else
    {
        SZG_ERROR("Failed to allocate transmittance LUT shader object.");
        return false;
    }

    return true;
}
auto populateSkyViewResources(
    VkDevice const device,
    VmaAllocator const allocator,
    syzygy::DescriptorAllocator& descriptorAllocator,
    syzygy::SkyViewComputePipeline::SkyViewLUTResources& resources
) -> bool
{
    VkExtent2D constexpr EXTENT_LUT{4096U, 4096U};

    if (auto mapResult{syzygy::ImageView::allocate(
            device,
            allocator,
            syzygy::ImageAllocationParameters{
                .extent = EXTENT_LUT,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .usageFlags =
                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            },
            syzygy::ImageViewAllocationParameters{}
        )};
        mapResult.has_value())
    {
        resources.map = std::move(mapResult).value();
    }
    else
    {
        SZG_ERROR("Failed to allocate skyview LUT map.");
        return false;
    }

    {
        VkSamplerCreateInfo const transmittanceSamplerInfo{
            syzygy::samplerCreateInfo(
                static_cast<VkFlags>(0),
                VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                VK_FILTER_LINEAR,
                // VK_FILTER_NEAREST,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
            )
        };

        SZG_TRY_VK(
            vkCreateSampler(
                device,
                &transmittanceSamplerInfo,
                nullptr,
                &resources.transmittanceImmutableSampler
            ),
            "Failed to create sampler for transmittance LUT",
            false
        );
    }

    if (auto setLayoutResult{
            syzygy::DescriptorLayoutBuilder{}
                .addBinding(
                    syzygy::DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    1
                )
                .addBinding(
                    syzygy::DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 1,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    {resources.transmittanceImmutableSampler}
                )
                .build(device, static_cast<VkFlags>(0))
        };
        setLayoutResult.has_value())
    {
        resources.setLayout = setLayoutResult.value();
    }
    else
    {
        SZG_ERROR("Failed to allocate skyview LUT descriptor set 0 layout.");
        return false;
    }

    resources.set = descriptorAllocator.allocate(device, resources.setLayout);

    std::array<VkDescriptorSetLayout, 1> const setLayouts{resources.setLayout};
    if (auto const& layoutResult{detail::createLayout(device, setLayouts, {})};
        layoutResult != VK_NULL_HANDLE)
    {
        resources.layout = layoutResult;
    }
    else
    {
        SZG_ERROR("Failed to allocate skyview LUT pipeline layout.");
        return false;
    }

    if (auto shaderResult{syzygy::loadShaderObject(
            device,
            "shaders/atmosphere/skyview_LUT.comp.spv",
            VK_SHADER_STAGE_COMPUTE_BIT,
            static_cast<VkFlags>(0),
            setLayouts,
            {}
        )};
        shaderResult.has_value())
    {
        resources.shader = shaderResult.value();
    }
    else
    {
        SZG_ERROR("Failed to allocate skyview LUT shader object.");
        return false;
    }

    return true;
}

auto populatePerspectiveResources(
    VkDevice const device,
    VmaAllocator const allocator,
    syzygy::DescriptorAllocator& descriptorAllocator,
    syzygy::SkyViewComputePipeline::PerspectiveMapResources& resources
) -> bool
{
    VkExtent2D constexpr EXTENT_MAP{2048U, 2048U};

    if (auto imageResult{syzygy::ImageView::allocate(
            device,
            allocator,
            syzygy::ImageAllocationParameters{
                .extent = EXTENT_MAP,
                .format = VK_FORMAT_R16G16B16A16_UNORM,
                .usageFlags = VK_IMAGE_USAGE_STORAGE_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            },
            syzygy::ImageViewAllocationParameters{}
        )};
        imageResult.has_value())
    {
        resources.outputImage = std::move(imageResult).value();
    }
    else
    {
        SZG_ERROR("Failed to allocate perspective map output image.");
        return false;
    }

    {
        VkSamplerCreateInfo const azimuthElevationMapSampler{
            syzygy::samplerCreateInfo(
                static_cast<VkFlags>(0),
                VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                VK_FILTER_LINEAR,
                // VK_FILTER_NEAREST,
                VK_SAMPLER_ADDRESS_MODE_REPEAT
            )
        };

        SZG_TRY_VK(
            vkCreateSampler(
                device,
                &azimuthElevationMapSampler,
                nullptr,
                &resources.azimuthElevationMapSampler
            ),
            "Failed to create sampler for perspective map.",
            false
        );
    }

    if (auto setLayoutResult{
            syzygy::DescriptorLayoutBuilder{}
                .addBinding(
                    syzygy::DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    1
                )
                .addBinding(
                    syzygy::DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 1,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    {resources.azimuthElevationMapSampler}
                )
                .build(device, static_cast<VkFlags>(0))
        };
        setLayoutResult.has_value())
    {
        resources.setLayout = setLayoutResult.value();
    }
    else
    {
        SZG_ERROR("Failed to allocate perspective map descriptor set 0 layout."
        );
        return false;
    }

    resources.set = descriptorAllocator.allocate(device, resources.setLayout);

    std::array<VkDescriptorSetLayout, 1> const setLayouts{resources.setLayout};
    if (auto shaderResult{syzygy::loadShaderObject(
            device,
            "shaders/atmosphere/camera.comp.spv",
            VK_SHADER_STAGE_COMPUTE_BIT,
            static_cast<VkFlags>(0),
            setLayouts,
            {}
        )};
        shaderResult.has_value())
    {
        resources.shader = shaderResult.value();
    }
    else
    {
        SZG_ERROR("Failed to allocate perspective map shader object.");
        return false;
    }

    std::array<VkPushConstantRange, 1> const pushConstants{
        resources.shader.reflectionData().defaultPushConstant().totalRange(
            VK_SHADER_STAGE_COMPUTE_BIT
        )
    };

    if (auto const& layoutResult{
            detail::createLayout(device, setLayouts, pushConstants)
        };
        layoutResult != VK_NULL_HANDLE)
    {
        resources.layout = layoutResult;
    }
    else
    {
        SZG_ERROR("Failed to allocate perspective map pipeline layout.");
        return false;
    }

    return true;
}

void updateDescriptors(
    VkDevice const device,
    syzygy::SkyViewComputePipeline::TransmittanceLUTResources const&
        transmittanceLUT,
    syzygy::SkyViewComputePipeline::SkyViewLUTResources const& skyviewLUT,
    syzygy::SkyViewComputePipeline::PerspectiveMapResources const&
        perspectiveMap
)
{
    { // Write transmittance descriptor
        VkDescriptorImageInfo const mapInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = transmittanceLUT.map->view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        std::array<VkWriteDescriptorSet, 1> writes{VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = transmittanceLUT.set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &mapInfo,
        }};

        vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
    }

    { // Write skyview descriptor
        VkDescriptorImageInfo const mapInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = skyviewLUT.map->view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkDescriptorImageInfo const transmittanceInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = transmittanceLUT.map->view(),
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        };

        std::array<VkWriteDescriptorSet, 2> writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = skyviewLUT.set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &mapInfo,
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = skyviewLUT.set,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &transmittanceInfo,
            }
        };

        vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
    }

    { // Write perspective map descriptor
        VkDescriptorImageInfo const mapInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = perspectiveMap.outputImage->view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkDescriptorImageInfo const skyviewInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = skyviewLUT.map->view(),
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        };

        std::array<VkWriteDescriptorSet, 2> writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = perspectiveMap.set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &mapInfo,
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = perspectiveMap.set,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &skyviewInfo,
            }
        };

        vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
    }
}

void recordPerspectiveMapCommands(
    VkCommandBuffer const cmd,
    syzygy::SkyViewComputePipeline::PerspectiveMapResources const& resources,
    syzygy::ImageView& skyViewLUT,
    VkExtent2D const drawExtent,
    uint32_t viewCameraIndex,
    syzygy::TStagedBuffer<syzygy::CameraPacked> const& cameras
)
{
    skyViewLUT.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    resources.outputImage->recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_GENERAL
    );

    // Perspective map shader
    VkShaderEXT const shader{resources.shader.shaderObject()};
    VkShaderStageFlagBits const stage{VK_SHADER_STAGE_COMPUTE_BIT};

    vkCmdBindShadersEXT(cmd, 1, &stage, &shader);

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        resources.layout,
        0,
        1,
        &resources.set,
        0,
        nullptr
    );

    syzygy::SkyViewComputePipeline::PerspectiveMapResources::PushConstant const
        pushConstant{
            .cameraBuffer = cameras.deviceAddress(),
            .cameraIndex = viewCameraIndex,
            .drawExtent = glm::uvec2{drawExtent.width, drawExtent.height},
        };
    vkCmdPushConstants(
        cmd,
        resources.layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(pushConstant),
        &pushConstant
    );

    uint32_t constexpr WORKGROUP_SIZE{16};

    vkCmdDispatch(
        cmd,
        detail::computeDispatchCount(drawExtent.width, WORKGROUP_SIZE),
        detail::computeDispatchCount(drawExtent.height, WORKGROUP_SIZE),
        1
    );
}

} // namespace detail

namespace syzygy
{
SkyViewComputePipeline::SkyViewComputePipeline(SkyViewComputePipeline&& other
) noexcept
{
    m_hasAllocations = std::exchange(other.m_hasAllocations, false);

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);

    m_descriptorAllocator = std::move(other.m_descriptorAllocator);

    m_transmittanceLUT = std::exchange(other.m_transmittanceLUT, {});
    m_skyViewLUT = std::exchange(other.m_skyViewLUT, {});
    m_perspectiveMap = std::exchange(other.m_perspectiveMap, {});
}
SkyViewComputePipeline::~SkyViewComputePipeline() { destroy(); }
auto SkyViewComputePipeline::create(
    VkDevice const device, VmaAllocator const allocator
) -> std::unique_ptr<SkyViewComputePipeline>
{
    std::unique_ptr<SkyViewComputePipeline> result{
        std::make_unique<SkyViewComputePipeline>(SkyViewComputePipeline{})
    };
    SkyViewComputePipeline& pipeline{*result};
    pipeline.m_hasAllocations = true;
    pipeline.m_device = device;

    std::array<DescriptorAllocator::PoolSizeRatio, 2> const poolRatios{
        DescriptorAllocator::PoolSizeRatio{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 0.5F
        },
        DescriptorAllocator::PoolSizeRatio{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 0.5F
        }
    };

    uint32_t constexpr MAX_SETS{10U};

    pipeline.m_descriptorAllocator =
        std::make_unique<DescriptorAllocator>(DescriptorAllocator::create(
            device, MAX_SETS, poolRatios, static_cast<VkFlags>(0)
        ));

    if (!detail::populateTransmittanceResources(
            device,
            allocator,
            *pipeline.m_descriptorAllocator,
            pipeline.m_transmittanceLUT
        ))
    {
        SZG_ERROR("Failed to allocate one or more Transmittance LUT resources."
        );
        return nullptr;
    }
    if (!detail::populateSkyViewResources(
            device,
            allocator,
            *pipeline.m_descriptorAllocator,
            pipeline.m_skyViewLUT
        ))
    {
        SZG_ERROR("Failed to allocate one or more SkyView LUT resources.");
        return nullptr;
    }
    if (!detail::populatePerspectiveResources(
            device,
            allocator,
            *pipeline.m_descriptorAllocator,
            pipeline.m_perspectiveMap
        ))
    {
        SZG_ERROR("Failed to allocate one or more perspective map resources.");
        return nullptr;
    }

    detail::updateDescriptors(
        device,
        pipeline.m_transmittanceLUT,
        pipeline.m_skyViewLUT,
        pipeline.m_perspectiveMap
    );

    return result;
}
void SkyViewComputePipeline::recordDrawCommands(
    VkCommandBuffer const cmd,
    VkRect2D const drawRect,
    syzygy::Image& color,
    uint32_t viewCameraIndex,
    TStagedBuffer<syzygy::CameraPacked> const& cameras
)
{
    // 1) Generate Transmittance LUT, a map of transmittance values in all
    // directions
    //
    // 2) Generate SkyView LUT, an azimuth-elevation map of what the
    // sky looks like (no tonemapping for now), consuming the transmittance LUT
    //
    // 3) PerspectiveMap performs the perspective transform of the SkyView LUT.
    // It consumes a camera + the SkyView LUT as a generic azimuth-elevation
    // map.

    VkShaderStageFlagBits const stage{VK_SHADER_STAGE_COMPUTE_BIT};
    uint32_t constexpr WORKGROUP_SIZE{16};

    m_transmittanceLUT.map->recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_GENERAL
    );

    {
        // Transmittance shader
        VkShaderEXT const transmittanceShader{
            m_transmittanceLUT.shader.shaderObject()
        };
        vkCmdBindShadersEXT(cmd, 1, &stage, &transmittanceShader);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_transmittanceLUT.layout,
            0,
            1,
            &m_transmittanceLUT.set,
            0,
            nullptr
        );

        VkExtent2D const transmittanceExtent{
            m_transmittanceLUT.map->image().extent2D()
        };
        vkCmdDispatch(
            cmd,
            detail::computeDispatchCount(
                transmittanceExtent.width, WORKGROUP_SIZE
            ),
            detail::computeDispatchCount(
                transmittanceExtent.height, WORKGROUP_SIZE
            ),
            1
        );
    }

    m_transmittanceLUT.map->recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    m_skyViewLUT.map->recordTransitionBarriered(cmd, VK_IMAGE_LAYOUT_GENERAL);

    {
        // Sky view shader
        VkShaderEXT const skyviewShader{m_skyViewLUT.shader.shaderObject()};

        vkCmdBindShadersEXT(cmd, 1, &stage, &skyviewShader);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_skyViewLUT.layout,
            0,
            1,
            &m_skyViewLUT.set,
            0,
            nullptr
        );

        VkExtent2D const skyViewExtent{m_skyViewLUT.map->image().extent2D()};
        vkCmdDispatch(
            cmd,
            detail::computeDispatchCount(skyViewExtent.width, WORKGROUP_SIZE),
            detail::computeDispatchCount(skyViewExtent.height, WORKGROUP_SIZE),
            1
        );
    }

    detail::recordPerspectiveMapCommands(
        cmd,
        m_perspectiveMap,
        *m_skyViewLUT.map,
        drawRect.extent,
        viewCameraIndex,
        cameras
    );

    m_perspectiveMap.outputImage->recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );
    color.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkOffset3D const srcMin{};
    VkOffset3D const srcMax{
        static_cast<int32_t>(drawRect.extent.width),
        static_cast<int32_t>(drawRect.extent.height),
        1
    };

    VkOffset3D const dstMin{drawRect.offset.x, drawRect.offset.y, 0};
    VkOffset3D const dstMax{
        static_cast<int32_t>(drawRect.extent.width) + dstMin.x,
        static_cast<int32_t>(drawRect.extent.height) + dstMin.y,
        1
    };

    Image::recordCopyRect(
        cmd,
        m_perspectiveMap.outputImage->image(),
        color,
        VK_IMAGE_ASPECT_COLOR_BIT,
        srcMin,
        srcMax,
        dstMin,
        dstMax
    );
}

void SkyViewComputePipeline::destroy()
{
    m_skyViewLUT.map.reset();
    m_transmittanceLUT.map.reset();

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_skyViewLUT.setLayout, nullptr);
        vkDestroyDescriptorSetLayout(
            m_device, m_transmittanceLUT.setLayout, nullptr
        );
        vkDestroyDescriptorSetLayout(
            m_device, m_perspectiveMap.setLayout, nullptr
        );

        vkDestroyPipelineLayout(m_device, m_skyViewLUT.layout, nullptr);
        vkDestroyPipelineLayout(m_device, m_transmittanceLUT.layout, nullptr);
        vkDestroyPipelineLayout(m_device, m_perspectiveMap.layout, nullptr);

        vkDestroySampler(
            m_device, m_skyViewLUT.transmittanceImmutableSampler, nullptr
        );
        vkDestroySampler(
            m_device, m_perspectiveMap.azimuthElevationMapSampler, nullptr
        );

        m_skyViewLUT.shader.cleanup(m_device);
        m_transmittanceLUT.shader.cleanup(m_device);
        m_perspectiveMap.shader.cleanup(m_device);
    }
    else if (m_hasAllocations)
    {
        SZG_ERROR("SkyViewComputePipeline had active allocations at "
                  "destruction time, but device was null.");
    }

    m_descriptorAllocator.reset();
}
} // namespace syzygy