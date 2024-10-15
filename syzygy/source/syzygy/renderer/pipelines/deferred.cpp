#include "deferred.hpp"

#include "syzygy/assets/assets.hpp"
#include "syzygy/assets/assetstypes.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/gbuffer.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/material.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include "syzygy/renderer/rendercommands.hpp"
#include "syzygy/renderer/scene.hpp"
#include "syzygy/renderer/scenetexture.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include <array>
#include <filesystem>
#include <functional>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <optional>
#include <spdlog/fmt/bundled/core.h>
#include <string>
#include <utility>
#include <vector>

namespace
{
void validatePushConstant(
    syzygy::ShaderObjectReflected const& shaderObject, size_t const expectedSize
)
{
    if (shaderObject.reflectionData().defaultEntryPointHasPushConstant())
    {
        syzygy::ShaderReflectionData::PushConstant const& pushConstant{
            shaderObject.reflectionData().defaultPushConstant()
        };

        size_t const loadedPushConstantSize{pushConstant.type.paddedSizeBytes};

        if (loadedPushConstantSize != expectedSize)
        {
            SZG_WARNING(fmt::format(
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
        SZG_WARNING(fmt::format(
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
) -> syzygy::ShaderObjectReflected
{
    std::optional<syzygy::ShaderObjectReflected> const loadResult{
        syzygy::loadShaderObject(
            device, path, stage, nextStage, descriptorSets, {}
        )
    };

    if (loadResult.has_value())
    {
        validatePushConstant(loadResult.value(), expectedPushConstantSize);
        return loadResult.value();
    }

    return syzygy::ShaderObjectReflected::makeInvalid();
}

auto loadShader(
    VkDevice const device,
    std::filesystem::path const& path,
    VkShaderStageFlagBits const stage,
    VkShaderStageFlags const nextStage,
    std::span<VkDescriptorSetLayout const> const descriptorSets,
    VkPushConstantRange const rangeOverride
) -> syzygy::ShaderObjectReflected
{
    std::optional<syzygy::ShaderObjectReflected> loadResult{
        syzygy::loadShaderObject(
            device, path, stage, nextStage, descriptorSets, rangeOverride, {}
        )
    };
    if (loadResult.has_value())
    {
        validatePushConstant(loadResult.value(), rangeOverride.size);
        return loadResult.value();
    }

    return syzygy::ShaderObjectReflected::makeInvalid();
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
        SZG_LOG_VK(result, "Creating shader object pipeline layout");
        return VK_NULL_HANDLE;
    }
    return layout;
}
} // namespace

namespace syzygy
{
DeferredShadingPipeline::DeferredShadingPipeline(
    VkDevice const device,
    VmaAllocator const allocator,
    SceneTexture const& sceneTexture,
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
        SZG_WARNING("Failed to create GBuffer for deferred shading pipeline.");
    }

    { // Lights used during the pass
        VkDeviceSize constexpr LIGHT_CAPACITY{16};

        m_spotLights =
            std::make_unique<syzygy::TStagedBuffer<syzygy::SpotLightPacked>>(
                syzygy::TStagedBuffer<syzygy::SpotLightPacked>::allocate(
                    device,
                    static_cast<VkBufferUsageFlags>(0),
                    allocator,
                    LIGHT_CAPACITY
                )
            );
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
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(GBufferVertexPushConstant),
        };

        VkDescriptorSetLayout const emptyDescriptorLayout{
            DescriptorLayoutBuilder{}
                .build(device, static_cast<VkFlags>(0))
                .value_or(VK_NULL_HANDLE)
        };

        VkDescriptorSetLayout const materialDataLayout{
            DescriptorLayoutBuilder{} // Color texture
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    1
                )
                // Normal texture
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 1,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    1
                )
                // ORM texture
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 2,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .bindingFlags = static_cast<VkFlags>(0),
                    },
                    1
                )
                .build(device, static_cast<VkFlags>(0))
                .value_or(VK_NULL_HANDLE)
        };

        std::vector<VkDescriptorSetLayout> const descriptorLayouts{
            emptyDescriptorLayout,
            emptyDescriptorLayout,
            emptyDescriptorLayout,
            materialDataLayout
        };

        m_gBufferVertexShader = loadShader(
            device,
            "shaders/deferred/offscreen.vert.spv",
            VK_SHADER_STAGE_VERTEX_BIT,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            descriptorLayouts,
            graphicsPushConstantRange
        );

        m_gBufferFragmentShader = loadShader(
            device,
            "shaders/deferred/offscreen.frag.spv",
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            descriptorLayouts,
            graphicsPushConstantRange
        );

        std::vector<VkPushConstantRange> const gBufferPushConstantRanges{
            graphicsPushConstantRange
        };
        m_gBufferLayout =
            createLayout(device, descriptorLayouts, gBufferPushConstantRanges);

        vkDestroyDescriptorSetLayout(device, emptyDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, materialDataLayout, nullptr);
    }

    { // Lighting pass pipeline
        if (auto samplerSetLayoutResult{
                ShadowPassArray::allocateSamplerSetLayout(device)
            };
            !samplerSetLayoutResult.has_value())
        {
            SZG_WARNING(
                "DeferredShadingPipeline: Failed to create sampler set layout."
            );
        }
        else
        {
            m_shadowPassArraySamplerSetLayout = samplerSetLayoutResult.value();
        }

        if (auto textureSetLayoutResult{
                ShadowPassArray::allocateTextureSetLayout(
                    device, SHADOWMAP_COUNT
                )
            };
            !textureSetLayoutResult.has_value())
        {
            SZG_WARNING(
                "DeferredShadingPipeline: Failed to create texture set layout."
            );
        }
        else
        {
            m_shadowPassArrayTextureSetLayout = textureSetLayoutResult.value();
        }

        std::vector<VkDescriptorSetLayout> const lightingPassDescriptorSets{
            sceneTexture.singletonLayout(),
            m_gBuffer.descriptorLayout,
            m_shadowPassArraySamplerSetLayout,
            m_shadowPassArrayTextureSetLayout
        };

        m_lightingPassComputeShader = loadShader(
            device,
            "shaders/deferred/lights.comp.spv",
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
}
} // namespace syzygy

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

auto collectGeometryCullFlags(
    VkCommandBuffer const cmd,
    VkPipelineStageFlags2 const bufferAccessStages,
    std::span<std::reference_wrapper<syzygy::MeshRenderResources> const>
        meshResources
) -> std::vector<syzygy::RenderOverride>
{
    std::vector<syzygy::RenderOverride> renderOverrides{};
    renderOverrides.reserve(meshResources.size());

    for (auto const& resources : meshResources)
    {
        syzygy::AssetShared<syzygy::Mesh> const meshAsset{
            resources.get().mesh.lock()
        };

        syzygy::RenderOverride const override{
            .render = meshAsset != nullptr && meshAsset->data != nullptr
                   && meshAsset->data->meshBuffers != nullptr
                   && resources.get().models != nullptr
                   && resources.get().modelInverseTransposes != nullptr
                   && resources.get().surfaceDescriptors.size()
                          >= meshAsset.get()->data->surfaces.size()
        };

        renderOverrides.push_back(override);

        if (!override.render)
        {
            continue;
        }

        resources.get().models->recordTotalCopyBarrier(
            cmd, bufferAccessStages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT
        );
        resources.get().modelInverseTransposes->recordTotalCopyBarrier(
            cmd, bufferAccessStages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT
        );
    }

    return renderOverrides;
}
} // namespace

namespace syzygy
{
void DeferredShadingPipeline::recordDrawCommands(
    VkCommandBuffer const cmd,
    VkRect2D const drawRect,
    SceneTexture& sceneTexture,
    uint32_t atmosphericDirectionalLightsCount,
    TStagedBuffer<DirectionalLightPacked> const& directionalLights,
    std::span<SpotLightPacked const> const spotLights,
    uint32_t const viewCameraIndex,
    TStagedBuffer<CameraPacked> const& cameras,
    std::span<std::reference_wrapper<MeshRenderResources> const> sceneGeometry
)
{
    VkPipelineStageFlags2 constexpr GBUFFER_ACCESS_STAGES{
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
        | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    };
    cameras.recordTotalCopyBarrier(
        cmd, GBUFFER_ACCESS_STAGES, VK_ACCESS_2_SHADER_STORAGE_READ_BIT
    );
    directionalLights.recordTotalCopyBarrier(
        cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT
    );

    { // Update lights
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

    std::vector<RenderOverride> renderOverrides{
        collectGeometryCullFlags(cmd, GBUFFER_ACCESS_STAGES, sceneGeometry)
    };

    { // Shadow maps
        m_shadowPassArray.recordInitialize(
            cmd,
            m_configuration.shadowPassParameters,
            directionalLights.readValidStaged(),
            m_spotLights->readValidStaged()
        );

        m_shadowPassArray.recordDrawCommands(
            cmd, sceneGeometry, renderOverrides
        );
    }

    { // Prepare GBuffer resources
        m_gBuffer.recordTransitionImages(
            cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );

        sceneTexture.depth().recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
        );
    }

    { // Deferred GBuffer pass
        setRasterizationShaderObjectState(
            cmd, VkRect2D{.extent{drawRect.extent}}
        );

        vkCmdSetCullModeEXT(cmd, VK_CULL_MODE_BACK_BIT);

        std::array<
            VkRenderingAttachmentInfo,
            GBuffer::GBUFFER_TEXTURE_COUNT> const gBufferAttachments{
            renderingAttachmentInfo(
                m_gBuffer.diffuseColor->view(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ),
            renderingAttachmentInfo(
                m_gBuffer.specularColor->view(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ),
            renderingAttachmentInfo(
                m_gBuffer.normal->view(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ),
            renderingAttachmentInfo(
                m_gBuffer.worldPosition->view(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ),
            renderingAttachmentInfo(
                m_gBuffer.occlusionRoughnessMetallic->view(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            )
        };

        VkRenderingAttachmentInfo const depthAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,

            .imageView = sceneTexture.depth().view(),
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
        std::array<VkColorComponentFlags, GBuffer::GBUFFER_TEXTURE_COUNT> const
            attachmentWriteMasks{
                colorComponentFlags,
                colorComponentFlags,
                colorComponentFlags,
                colorComponentFlags,
                colorComponentFlags
            };
        vkCmdSetColorWriteMaskEXT(cmd, 0, VKR_ARRAY(attachmentWriteMasks));

        std::array<VkBool32, GBuffer::GBUFFER_TEXTURE_COUNT> const
            colorBlendEnabled{VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE};
        vkCmdSetColorBlendEnableEXT(cmd, 0, VKR_ARRAY(colorBlendEnabled));

        VkRenderingInfo const renderInfo{renderingInfo(
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
        std::array<VkClearAttachment, GBuffer::GBUFFER_TEXTURE_COUNT> const
            clearAttachments{
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
                },
                VkClearAttachment{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .colorAttachment = 4,
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

        for (size_t index{0}; index < sceneGeometry.size(); index++)
        {
            MeshRenderResources const& instanceResources{
                sceneGeometry[index].get()
            };

            if (index < renderOverrides.size()
                && !renderOverrides[index].render)
            {
                continue;
            }

            Mesh const& meshAsset{*instanceResources.mesh.lock()->data};

            TStagedBuffer<glm::mat4x4> const& models{*instanceResources.models};
            TStagedBuffer<glm::mat4x4> const& modelInverseTransposes{
                *instanceResources.modelInverseTransposes
            };

            GPUMeshBuffers& meshBuffers{*meshAsset.meshBuffers};

            { // Vertex push constant
                GBufferVertexPushConstant const vertexPushConstant{
                    .vertexBuffer = meshBuffers.vertexAddress(),
                    .modelBuffer = models.deviceAddress(),
                    .modelInverseTransposeBuffer =
                        modelInverseTransposes.deviceAddress(),
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

            std::span<MaterialDescriptors const> const surfaceDescriptors{
                instanceResources.surfaceDescriptors
            };
            for (size_t surfaceIndex{0};
                 surfaceIndex < std::min(
                     meshAsset.surfaces.size(), surfaceDescriptors.size()
                 );
                 surfaceIndex++)
            {
                GeometrySurface const& drawnSurface{
                    meshAsset.surfaces[surfaceIndex]
                };
                MaterialDescriptors const& descriptors{
                    surfaceDescriptors[surfaceIndex]
                };

                descriptors.bind(cmd, m_gBufferLayout, 3);

                // Bind the entire index buffer of the mesh, but only draw a
                // single surface.
                vkCmdBindIndexBuffer(
                    cmd, meshBuffers.indexBuffer(), 0, VK_INDEX_TYPE_UINT32
                );
                vkCmdDrawIndexed(
                    cmd,
                    drawnSurface.indexCount,
                    models.deviceSize(),
                    drawnSurface.firstIndex,
                    0,
                    0
                );
            }
        }

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

    recordClearColorImage(
        cmd, sceneTexture.color().image(), COLOR_BLACK_OPAQUE
    );

    { // Lighting pass using GBuffer output
        m_gBuffer.recordTransitionImages(
            cmd, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
        );

        sceneTexture.color().recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_GENERAL
        );

        m_shadowPassArray.recordTransitionActiveShadowMaps(
            cmd, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
        );

        VkShaderStageFlagBits const computeStage{VK_SHADER_STAGE_COMPUTE_BIT};
        VkShaderEXT const shader{m_lightingPassComputeShader.shaderObject()};
        vkCmdBindShadersEXT(cmd, 1, &computeStage, &shader);

        std::array<VkDescriptorSet, 4> descriptorSets{
            sceneTexture.singletonDescriptor(),
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

            .directionalLightsBuffer = directionalLights.deviceAddress(),
            .spotLightsBuffer = m_spotLights->deviceAddress(),

            .directionalLightCount =
                static_cast<uint32_t>(directionalLights.deviceSize()),
            .spotLightCount = static_cast<uint32_t>(m_spotLights->deviceSize()),
            .directionalLightSkipCount = atmosphericDirectionalLightsCount,
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
}

auto DeferredShadingPipeline::gbuffer() -> GBuffer const& { return m_gBuffer; }

auto DeferredShadingPipeline::shadowMaps() -> ShadowPassArray const&
{
    return m_shadowPassArray;
}

void DeferredShadingPipeline::cleanup(
    VkDevice const device, VmaAllocator const allocator
)
{
    m_shadowPassArray.cleanup(device, allocator);
    m_gBuffer.cleanup(device);

    m_spotLights.reset();

    vkDestroyDescriptorSetLayout(
        device, m_shadowPassArraySamplerSetLayout, nullptr
    );
    vkDestroyDescriptorSetLayout(
        device, m_shadowPassArrayTextureSetLayout, nullptr
    );

    vkDestroyPipelineLayout(device, m_gBufferLayout, nullptr);
    vkDestroyPipelineLayout(device, m_lightingPassLayout, nullptr);

    m_gBufferVertexShader.cleanup(device);
    m_gBufferFragmentShader.cleanup(device);
    m_lightingPassComputeShader.cleanup(device);
}
auto DeferredShadingPipeline::getConfiguration() const -> Configuration
{
    return m_configuration;
}
void DeferredShadingPipeline::setConfiguration(Configuration const parameters)
{
    m_configuration = parameters;
}
} // namespace syzygy
