#include "deferred.hpp"

#include "../initializers.hpp"
#include "../renderpass/renderpass.hpp"

namespace
{
void validatePushConstant(
    ShaderObjectReflected const& shaderObject, size_t const expectedSize
)
{
    if (shaderObject.reflectionData().defaultEntryPointHasPushConstant())
    {
        ShaderReflectionData::PushConstant const& pushConstant{
            shaderObject.reflectionData().defaultPushConstant()
        };

        size_t const loadedPushConstantSize{pushConstant.type.paddedSizeBytes};

        if (loadedPushConstantSize != expectedSize)
        {
            Warning(fmt::format(
                "Loaded Shader \"{}\" had a push constant of size {}, "
                "while implementation expects {}.",
                shaderObject.name(),
                loadedPushConstantSize,
                expectedSize
            ));
        }
    }
    else if (expectedSize > 0)
    {
        Warning(fmt::format(
            "Loaded Shader \"{}\" had no push constant, "
            "while implementation expects one of size {}.",
            shaderObject.name(),
            expectedSize
        ));
    }
}

auto loadShader(
    VkDevice const device,
    std::string const& path,
    VkShaderStageFlagBits const stage,
    VkShaderStageFlags const nextStage,
    std::span<VkDescriptorSetLayout const> const descriptorSets,
    size_t const expectedPushConstantSize
) -> ShaderObjectReflected
{
    std::optional<ShaderObjectReflected> const loadResult{
        vkutil::loadShaderObject(
            device, path, stage, nextStage, descriptorSets, {}
        )
    };

    if (loadResult.has_value())
    {
        validatePushConstant(loadResult.value(), expectedPushConstantSize);
        return loadResult.value();
    }

    return ShaderObjectReflected::makeInvalid();
}

auto loadShader(
    VkDevice const device,
    std::string const& path,
    VkShaderStageFlagBits const stage,
    VkShaderStageFlags const nextStage,
    std::span<VkDescriptorSetLayout const> const descriptorSets,
    VkPushConstantRange const rangeOverride
) -> ShaderObjectReflected
{
    std::optional<ShaderObjectReflected> loadResult{vkutil::loadShaderObject(
        device, path, stage, nextStage, descriptorSets, rangeOverride, {}
    )};
    if (loadResult.has_value())
    {
        validatePushConstant(loadResult.value(), rangeOverride.size);
        return loadResult.value();
    }

    return ShaderObjectReflected::makeInvalid();
}

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
        LogVkResult(result, "Creating shader object pipeline layout");
        return VK_NULL_HANDLE;
    }
    return layout;
}
} // namespace

DeferredShadingPipeline::DeferredShadingPipeline(
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator,
    VkExtent2D const dimensionCapacity
)
{
    if (std::optional<GBuffer> gBufferResult{GBuffer::create(
            device, dimensionCapacity, allocator, descriptorAllocator
        )};
        gBufferResult.has_value())
    {
        m_gBuffer = std::move(gBufferResult).value();
    }
    else
    {
        Warning("Failed to create GBuffer for deferred shading pipeline.");
    }

    { // Lights used during the pass
        VkDeviceSize constexpr LIGHT_CAPACITY{16};

        m_directionalLights =
            std::make_unique<TStagedBuffer<gputypes::LightDirectional>>(
                TStagedBuffer<gputypes::LightDirectional>::allocate(
                    device, allocator, LIGHT_CAPACITY, 0
                )
            );
        m_spotLights = std::make_unique<TStagedBuffer<gputypes::LightSpot>>(
            TStagedBuffer<gputypes::LightSpot>::allocate(
                device, allocator, LIGHT_CAPACITY, 0
            )
        );
    }

    { // Descriptor Sets
        m_drawImageLayout =
            DescriptorLayoutBuilder()
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = 0,
                    },
                    1U
                )
                .build(device, 0)
                .value_or(VK_NULL_HANDLE);

        m_drawImageSet =
            descriptorAllocator.allocate(device, m_drawImageLayout);

        {
            VkExtent2D const drawImageExtent{
                .width = dimensionCapacity.width,
                .height = dimensionCapacity.height,
            };

            if (std::optional<AllocatedImage> drawImageResult{
                    AllocatedImage::allocate(
                        allocator,
                        device,
                        AllocatedImage::AllocationParameters{
                            .extent = drawImageExtent,
                            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                            .usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                        | VK_IMAGE_USAGE_STORAGE_BIT
                                        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                            .viewFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
                        }
                    )
                };
                drawImageResult.has_value())
            {
                m_drawImage = std::make_unique<AllocatedImage>(
                    std::move(drawImageResult).value()
                );
            }
            else
            {
                Warning("Failed to allocate draw image for deferred shading "
                        "pipeline.");
            }

            VkDescriptorImageInfo const drawImageInfo{
                .sampler = VK_NULL_HANDLE,
                .imageView = m_drawImage->view(),
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            };

            VkWriteDescriptorSet const drawImageWrite{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,

                .dstSet = m_drawImageSet,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,

                .pImageInfo = &drawImageInfo,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr,
            };

            std::vector<VkWriteDescriptorSet> const writes{drawImageWrite};

            vkUpdateDescriptorSets(device, VKR_ARRAY(writes), 0, nullptr);
        }

        VkSamplerCreateInfo const depthImageImmutableSamplerInfo{
            vkinit::samplerCreateInfo(
                0,
                VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                VK_FILTER_NEAREST,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
            )
        };

        LogVkResult(
            vkCreateSampler(
                device,
                &depthImageImmutableSamplerInfo,
                nullptr,
                &m_depthImageImmutableSampler
            ),
            "Creating depth sampler for deferred shading"
        );

        m_depthImageLayout =
            DescriptorLayoutBuilder()
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = 0,
                    },
                    {m_depthImageImmutableSampler}
                )
                .build(device, 0)
                .value_or(VK_NULL_HANDLE);

        m_depthImageSet =
            descriptorAllocator.allocate(device, m_depthImageLayout);
    }

    uint32_t constexpr SHADOWMAP_SIZE{8192};
    size_t constexpr SHADOWMAP_COUNT{10};

    m_shadowPassArray =
        ShadowPassArray::create( // NOLINT(bugprone-unchecked-optional-access):
                                 // Necessary for program execution
            device,
            descriptorAllocator,
            allocator,
            VkExtent2D{
                .width = SHADOWMAP_SIZE,
                .height = SHADOWMAP_SIZE,
            },
            SHADOWMAP_COUNT
        )
            .value();

    { // GBuffer pipelines
        VkPushConstantRange const graphicsPushConstantRange{
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(GBufferVertexPushConstant),
        };

        m_gBufferVertexShader = loadShader(
            device,
            "shaders/deferred/offscreen.vert.spv",
            VK_SHADER_STAGE_VERTEX_BIT,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            {},
            graphicsPushConstantRange
        );

        m_gBufferFragmentShader = loadShader(
            device,
            "shaders/deferred/offscreen.frag.spv",
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            {},
            graphicsPushConstantRange
        );

        std::vector<VkPushConstantRange> const gBufferPushConstantRanges{
            VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(GBufferVertexPushConstant),
            }
        };
        m_gBufferLayout = createLayout(device, {}, gBufferPushConstantRanges);
    }

    { // Lighting pass pipeline
        std::vector<VkDescriptorSetLayout> const lightingPassDescriptorSets{
            m_drawImageLayout,
            m_gBuffer.descriptorLayout,
            m_shadowPassArray.samplerSetLayout(),
            m_shadowPassArray.texturesSetLayout()
        };

        m_lightingPassComputeShader = loadShader(
            device,
            "shaders/deferred/directional_light.comp.spv",
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            lightingPassDescriptorSets,
            sizeof(LightingPassComputePushConstant)
        );

        std::vector<VkPushConstantRange> const lightingPassPushConstantRanges{
            VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = sizeof(LightingPassComputePushConstant),
            }
        };
        m_lightingPassLayout = createLayout(
            device, lightingPassDescriptorSets, lightingPassPushConstantRanges
        );
    }

    { // Sky pass pipeline
        std::vector<VkDescriptorSetLayout> const skyPassDescriptorSets{
            m_drawImageLayout, m_depthImageLayout
        };

        m_skyPassComputeShader = loadShader(
            device,
            "shaders/deferred/sky.comp.spv",
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            skyPassDescriptorSets,
            sizeof(SkyPassComputePushConstant)
        );

        std::vector<VkPushConstantRange> const skyPassPushConstantRanges{
            VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = sizeof(SkyPassComputePushConstant),
            }
        };
        m_skyPassLayout = createLayout(
            device, skyPassDescriptorSets, skyPassPushConstantRanges
        );
    }
}

namespace
{
void setRasterizationShaderObjectState(
    VkCommandBuffer const cmd, VkRect2D const drawRect
)
{
    VkViewport const viewport{
        .x = static_cast<float>(drawRect.offset.x),
        .y = static_cast<float>(drawRect.offset.y),
        .width = static_cast<float>(drawRect.extent.width),
        .height = static_cast<float>(drawRect.extent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F,
    };

    vkCmdSetViewportWithCount(cmd, 1, &viewport);

    VkRect2D const scissor{drawRect};

    vkCmdSetScissorWithCount(cmd, 1, &scissor);

    vkCmdSetRasterizerDiscardEnable(cmd, VK_FALSE);

    VkColorBlendEquationEXT const colorBlendEquation{};
    vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &colorBlendEquation);

    // No vertex input state since we use buffer addresses

    vkCmdSetCullModeEXT(cmd, VK_CULL_MODE_NONE);

    vkCmdSetPrimitiveTopology(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    vkCmdSetPrimitiveRestartEnable(cmd, VK_FALSE);
    vkCmdSetRasterizationSamplesEXT(cmd, VK_SAMPLE_COUNT_1_BIT);

    VkSampleMask const sampleMask{0b1};
    vkCmdSetSampleMaskEXT(cmd, VK_SAMPLE_COUNT_1_BIT, &sampleMask);

    vkCmdSetAlphaToCoverageEnableEXT(cmd, VK_FALSE);

    vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_FILL);

    vkCmdSetFrontFace(cmd, VK_FRONT_FACE_CLOCKWISE);

    vkCmdSetDepthWriteEnable(cmd, VK_TRUE);

    vkCmdSetDepthTestEnable(cmd, VK_TRUE);
    vkCmdSetDepthCompareOpEXT(cmd, VK_COMPARE_OP_GREATER);

    vkCmdSetDepthBoundsTestEnable(cmd, VK_FALSE);
    vkCmdSetDepthBiasEnableEXT(cmd, VK_FALSE);

    vkCmdSetStencilTestEnable(cmd, VK_FALSE);
}
} // namespace

void DeferredShadingPipeline::recordDrawCommands(
    VkCommandBuffer const cmd,
    VkRect2D const drawRect,
    AllocatedImage& color,
    AllocatedImage& depth,
    std::span<gputypes::LightDirectional const> const directionalLights,
    std::span<gputypes::LightSpot const> const spotLights,
    uint32_t const viewCameraIndex,
    TStagedBuffer<gputypes::Camera> const& cameras,
    uint32_t const atmosphereIndex,
    TStagedBuffer<gputypes::Atmosphere> const& atmospheres,
    scene::MeshInstanced const& sceneGeometry
)
{
    VkPipelineStageFlags2 const bufferStages{
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
        | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    };
    cameras.recordTotalCopyBarrier(
        cmd, bufferStages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT
    );
    atmospheres.recordTotalCopyBarrier(
        cmd, bufferStages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT
    );
    sceneGeometry.models->recordTotalCopyBarrier(
        cmd, bufferStages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT
    );
    sceneGeometry.modelInverseTransposes->recordTotalCopyBarrier(
        cmd, bufferStages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT
    );

    { // Update lights
        if (!directionalLights.empty())
        {
            m_directionalLights->clearStaged();
            m_directionalLights->push(directionalLights);
            m_directionalLights->recordCopyToDevice(cmd);
            m_directionalLights->recordTotalCopyBarrier(
                cmd,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT
            );
        }
        else
        {
            m_directionalLights->clearStagedAndDevice();
        }

        if (!spotLights.empty())
        {
            m_spotLights->clearStaged();
            m_spotLights->push(spotLights);
            m_spotLights->recordCopyToDevice(cmd);
            m_spotLights->recordTotalCopyBarrier(
                cmd,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT
            );
        }
        else
        {
            m_spotLights->clearStagedAndDevice();
        }
    }

    bool const shouldRenderGeometry{
        sceneGeometry.render || sceneGeometry.mesh == nullptr
    };
    if (shouldRenderGeometry)
    { // Shadow maps
        m_shadowPassArray.recordInitialize(
            cmd,
            m_parameters.shadowPassParameters,
            m_directionalLights->readValidStaged(),
            m_spotLights->readValidStaged()
        );

        m_shadowPassArray.recordDrawCommands(
            cmd, *sceneGeometry.mesh, *sceneGeometry.models
        );
    }

    if (shouldRenderGeometry)
    { // Prepare GBuffer resources
        m_gBuffer.recordTransitionImages(
            cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );

        depth.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
        );
    }

    if (shouldRenderGeometry)
    { // Deferred GBuffer pass
        setRasterizationShaderObjectState(
            cmd, VkRect2D{.extent{drawRect.extent}}
        );

        vkCmdSetCullModeEXT(cmd, VK_CULL_MODE_BACK_BIT);

        std::array<VkRenderingAttachmentInfo, 4> const gBufferAttachments{
            vkinit::renderingAttachmentInfo(
                m_gBuffer.diffuseColor->view(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ),
            vkinit::renderingAttachmentInfo(
                m_gBuffer.specularColor->view(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ),
            vkinit::renderingAttachmentInfo(
                m_gBuffer.normal->view(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ),
            vkinit::renderingAttachmentInfo(
                m_gBuffer.worldPosition->view(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            )
        };

        VkRenderingAttachmentInfo const depthAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,

            .imageView = depth.view(),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,

            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,

            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,

            .clearValue{VkClearValue{.depthStencil{.depth = 0.0F}}},
        };

        VkColorComponentFlags const colorComponentFlags{
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };
        std::array<VkColorComponentFlags, 4> const attachmentWriteMasks{
            colorComponentFlags,
            colorComponentFlags,
            colorComponentFlags,
            colorComponentFlags
        };
        vkCmdSetColorWriteMaskEXT(cmd, 0, VKR_ARRAY(attachmentWriteMasks));

        std::array<VkBool32, 4> const colorBlendEnabled{
            VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE
        };
        vkCmdSetColorBlendEnableEXT(cmd, 0, VKR_ARRAY(colorBlendEnabled));

        VkRenderingInfo const renderInfo{vkinit::renderingInfo(
            VkRect2D{.extent{drawRect.extent}},
            gBufferAttachments,
            &depthAttachment
        )};

        std::array<VkShaderStageFlagBits, 2> const stages{
            VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT
        };
        std::array<VkShaderEXT, 2> const shaders{
            m_gBufferVertexShader.shaderObject(),
            m_gBufferFragmentShader.shaderObject()
        };

        vkCmdBeginRendering(cmd, &renderInfo);

        VkClearValue const clearColor{.color{.float32{0.0, 0.0, 0.0, 0.0}}};
        std::array<VkClearAttachment, 4> const clearAttachments{
            VkClearAttachment{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = 0,
                .clearValue = clearColor,
            },
            VkClearAttachment{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = 1,
                .clearValue = clearColor,
            },
            VkClearAttachment{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = 2,
                .clearValue = clearColor,
            },
            VkClearAttachment{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = 3,
                .clearValue = clearColor,
            }
        };
        VkClearRect const clearRect{
            .rect = VkRect2D{.extent{drawRect.extent}},
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        vkCmdClearAttachments(cmd, VKR_ARRAY(clearAttachments), 1, &clearRect);

        vkCmdBindShadersEXT(cmd, 2, stages.data(), shaders.data());

        GPUMeshBuffers& meshBuffers{*sceneGeometry.mesh->meshBuffers};

        { // Vertex push constant
            GBufferVertexPushConstant const vertexPushConstant{
                .vertexBuffer = meshBuffers.vertexAddress(),
                .modelBuffer = sceneGeometry.models->deviceAddress(),
                .modelInverseTransposeBuffer =
                    sceneGeometry.modelInverseTransposes->deviceAddress(),
                .cameraBuffer = cameras.deviceAddress(),
                .cameraIndex = viewCameraIndex,
            };
            vkCmdPushConstants(
                cmd,
                m_gBufferLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(GBufferVertexPushConstant),
                &vertexPushConstant
            );
        }

        GeometrySurface const& drawnSurface{sceneGeometry.mesh->surfaces[0]};

        // Bind the entire index buffer of the mesh, but only draw a single
        // surface.
        vkCmdBindIndexBuffer(
            cmd, meshBuffers.indexBuffer(), 0, VK_INDEX_TYPE_UINT32
        );
        vkCmdDrawIndexed(
            cmd,
            drawnSurface.indexCount,
            sceneGeometry.models->deviceSize(),
            drawnSurface.firstIndex,
            0,
            0
        );

        std::array<VkShaderStageFlagBits, 2> const unboundStages{
            VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT
        };
        std::array<VkShaderEXT, 2> const unboundHandles{
            VK_NULL_HANDLE, VK_NULL_HANDLE
        };
        vkCmdBindShadersEXT(
            cmd, VKR_ARRAY(unboundStages), unboundHandles.data()
        );

        vkCmdEndRendering(cmd);
    }
    else
    {
        renderpass::recordClearDepthImage(
            cmd, depth, VkClearDepthStencilValue{.depth = renderpass::DEPTH_FAR}
        );
    }

    renderpass::recordClearColorImage(
        cmd, color, renderpass::COLOR_BLACK_OPAQUE
    );

    if (shouldRenderGeometry)
    { // Lighting pass using GBuffer output
        m_gBuffer.recordTransitionImages(
            cmd, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
        );

        m_drawImage->recordTransitionBarriered(cmd, VK_IMAGE_LAYOUT_GENERAL);

        m_shadowPassArray.recordTransitionActiveShadowMaps(
            cmd, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
        );

        VkShaderStageFlagBits const computeStage{VK_SHADER_STAGE_COMPUTE_BIT};
        VkShaderEXT const shader{m_lightingPassComputeShader.shaderObject()};
        vkCmdBindShadersEXT(cmd, 1, &computeStage, &shader);

        std::array<VkDescriptorSet, 4> descriptorSets{
            m_drawImageSet,
            m_gBuffer.descriptors,
            m_shadowPassArray.samplerSet(),
            m_shadowPassArray.textureSet()
        };

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_lightingPassLayout,
            0,
            VKR_ARRAY(descriptorSets),
            0,
            nullptr
        );

        LightingPassComputePushConstant const pushConstant{
            .cameraBuffer = cameras.deviceAddress(),
            .atmosphereBuffer = atmospheres.deviceAddress(),

            .directionalLightsBuffer = m_directionalLights->deviceAddress(),
            .spotLightsBuffer = m_spotLights->deviceAddress(),

            .directionalLightCount =
                static_cast<uint32_t>(m_directionalLights->deviceSize()),
            .spotLightCount = static_cast<uint32_t>(m_spotLights->deviceSize()),
            .atmosphereIndex = atmosphereIndex,
            .cameraIndex = viewCameraIndex,
            .gbufferOffset = glm::vec2{0.0, 0.0},
            .gbufferExtent =
                glm::vec2(m_gBuffer.extent().width, m_gBuffer.extent().height),
        };

        vkCmdPushConstants(
            cmd,
            m_lightingPassLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(LightingPassComputePushConstant),
            &pushConstant
        );

        uint32_t constexpr COMPUTE_WORKGROUP_SIZE{16};

        vkCmdDispatch(
            cmd,
            computeDispatchCount(drawRect.extent.width, COMPUTE_WORKGROUP_SIZE),
            computeDispatchCount(
                drawRect.extent.height, COMPUTE_WORKGROUP_SIZE
            ),
            1
        );

        VkShaderEXT const unboundHandle{VK_NULL_HANDLE};
        vkCmdBindShadersEXT(cmd, 1, &computeStage, &unboundHandle);
    }

    { // Sky post-process pass
        m_drawImage->recordTransitionBarriered(cmd, VK_IMAGE_LAYOUT_GENERAL);
        depth.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
        );

        VkShaderStageFlagBits const computeStage{VK_SHADER_STAGE_COMPUTE_BIT};
        VkShaderEXT const shader{m_skyPassComputeShader.shaderObject()};
        vkCmdBindShadersEXT(cmd, 1, &computeStage, &shader);

        std::array<VkDescriptorSet, 2> const descriptorSets{
            m_drawImageSet, m_depthImageSet
        };

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_skyPassLayout,
            0,
            VKR_ARRAY(descriptorSets),
            0,
            nullptr
        );

        SkyPassComputePushConstant const pushConstant{
            .atmosphereBuffer = atmospheres.deviceAddress(),
            .cameraBuffer = cameras.deviceAddress(),
            .atmosphereIndex = atmosphereIndex,
            .cameraIndex = viewCameraIndex,
            .drawOffset = glm::vec2{0.0, 0.0},
            .drawExtent =
                glm::vec2{drawRect.extent.width, drawRect.extent.height},
        };

        vkCmdPushConstants(
            cmd,
            m_skyPassLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(SkyPassComputePushConstant),
            &pushConstant
        );

        uint32_t constexpr COMPUTE_WORKGROUP_SIZE{16};

        vkCmdDispatch(
            cmd,
            computeDispatchCount(drawRect.extent.width, COMPUTE_WORKGROUP_SIZE),
            computeDispatchCount(
                drawRect.extent.height, COMPUTE_WORKGROUP_SIZE
            ),
            1
        );

        VkShaderEXT const unboundHandle{VK_NULL_HANDLE};
        vkCmdBindShadersEXT(cmd, 1, &computeStage, &unboundHandle);
    }

    {
        m_drawImage->recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        );
        color.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        VkRect2D const srcRegion{.offset{}, .extent{drawRect.extent}};
        VkRect2D const dstRegion{drawRect};
        AllocatedImage::recordCopySubregion(
            cmd, *m_drawImage, srcRegion, color, dstRegion
        );
    }
}

void DeferredShadingPipeline::updateRenderTargetDescriptors(
    VkDevice const device, AllocatedImage& depthImage
)
{
    VkDescriptorImageInfo const depthImageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = depthImage.view(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet const depthImageWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = m_depthImageSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

        .pImageInfo = &depthImageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    std::vector<VkWriteDescriptorSet> const writes{depthImageWrite};

    vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
}

void DeferredShadingPipeline::cleanup(
    VkDevice const device, VmaAllocator const allocator
)
{
    m_shadowPassArray.cleanup(device, allocator);
    m_gBuffer.cleanup(device);

    m_directionalLights.reset();
    m_spotLights.reset();

    m_drawImage.reset();

    vkDestroyDescriptorSetLayout(device, m_depthImageLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_drawImageLayout, nullptr);

    vkDestroySampler(device, m_depthImageImmutableSampler, nullptr);

    vkDestroyPipelineLayout(device, m_gBufferLayout, nullptr);
    vkDestroyPipelineLayout(device, m_lightingPassLayout, nullptr);
    vkDestroyPipelineLayout(device, m_skyPassLayout, nullptr);

    m_gBufferVertexShader.cleanup(device);
    m_gBufferFragmentShader.cleanup(device);
    m_lightingPassComputeShader.cleanup(device);
    m_skyPassComputeShader.cleanup(device);
}
