#include "pipelines.hpp"

#include "syzygy/assets.hpp"
#include "syzygy/buffers.hpp"
#include "syzygy/core/scene.hpp"
#include "syzygy/enginetypes.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/images/image.hpp"
#include "syzygy/images/imageview.hpp"
#include "syzygy/initializers.hpp"
#include "syzygy/pipelines.hpp"
#include "syzygy/shaders.hpp"
#include <glm/vec2.hpp>
#include <memory>
#include <utility>

namespace gputypes
{
struct Camera;
} // namespace gputypes

auto PipelineBuilder::buildPipeline(
    VkDevice const device, VkPipelineLayout const layout
) const -> VkPipeline
{
    VkPipelineViewportStateCreateInfo const viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,

        .viewportCount = 1,
        .scissorCount = 1,

        // We use dynamic rendering, so no other members are needed
    };

    std::vector<VkFormat> colorFormats{};
    std::vector<VkPipelineColorBlendAttachmentState> attachmentStates{};
    if (m_colorAttachment.has_value())
    {
        ColorAttachmentSpecification const specification{
            m_colorAttachment.value()
        };

        colorFormats.push_back(specification.format);
        attachmentStates.push_back(specification.blending);
    }

    VkPipelineColorBlendStateCreateInfo const colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,

        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,

        .attachmentCount = static_cast<uint32_t>(attachmentStates.size()),
        .pAttachments = attachmentStates.data(),
    };

    VkPipelineRenderingCreateInfo const renderInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,

        .colorAttachmentCount = static_cast<uint32_t>(colorFormats.size()),
        .pColorAttachmentFormats = colorFormats.data(),

        .depthAttachmentFormat = m_depthAttachmentFormat,
    };

    // Dummy vertex input
    VkPipelineVertexInputStateCreateInfo const vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    std::vector<VkDynamicState> dynamicStates{
        m_dynamicStates.begin(), m_dynamicStates.end()
    };

    // We insert these by default since we have no methods for setting
    // the static state for now
    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo const dynamicInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    VkGraphicsPipelineCreateInfo const pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,

        .stageCount = static_cast<uint32_t>(m_shaderStages.size()),
        .pStages = m_shaderStages.data(),

        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &m_inputAssembly,

        .pTessellationState = nullptr,

        .pViewportState = &viewportState,
        .pRasterizationState = &m_rasterizer,
        .pMultisampleState = &m_multisampling,

        .pDepthStencilState = &m_depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicInfo,

        .layout = layout,
        .renderPass = VK_NULL_HANDLE, // dynamic rendering used
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VkPipeline pipeline{VK_NULL_HANDLE};
    SZG_LOG_VK(
        vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline
        ),
        "Building graphics pipeline"
    );
    return pipeline;
}

void PipelineBuilder::pushShader(
    ShaderModuleReflected const& shader, VkShaderStageFlagBits const stage
)
{
    m_shaderStages.push_back(vkinit::pipelineShaderStageCreateInfo(
        stage, shader.shaderModule(), shader.reflectionData().defaultEntryPoint
    ));
}

void PipelineBuilder::setInputTopology(VkPrimitiveTopology const topology)
{
    m_inputAssembly.topology = topology;
    m_inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::setPolygonMode(VkPolygonMode const mode)
{
    m_rasterizer.polygonMode = mode;
}

void PipelineBuilder::pushDynamicState(VkDynamicState const dynamicState)
{
    m_dynamicStates.insert(dynamicState);
}

void PipelineBuilder::setCullMode(
    VkCullModeFlags const cullMode, VkFrontFace const frontFace
)
{
    m_rasterizer.cullMode = cullMode;
    m_rasterizer.frontFace = frontFace;
}

void PipelineBuilder::setMultisamplingNone()
{
    m_multisampling.sampleShadingEnable = VK_FALSE;
    m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_multisampling.minSampleShading = 1.0F;
    m_multisampling.pSampleMask = nullptr;
    m_multisampling.alphaToCoverageEnable = VK_FALSE;
    m_multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::setColorAttachment(VkFormat const format)
{
    m_colorAttachment = ColorAttachmentSpecification{
        .format = format,
    };
}

void PipelineBuilder::setDepthFormat(VkFormat const format)
{
    m_depthAttachmentFormat = format;
}

void PipelineBuilder::enableDepthBias()
{
    m_rasterizer.depthBiasEnable = VK_TRUE;
}

void PipelineBuilder::disableDepthTest()
{
    m_depthStencil = VkPipelineDepthStencilStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_NEVER,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0F,
        .maxDepthBounds = 1.0F,
    };
}

void PipelineBuilder::enableDepthTest(
    bool const depthWriteEnable, VkCompareOp const compareOp
)
{
    m_depthStencil = VkPipelineDepthStencilStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = depthWriteEnable ? VK_TRUE : VK_FALSE,
        .depthCompareOp = compareOp,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0F,
        .maxDepthBounds = 1.0F,
    };
}

ComputeCollectionPipeline::ComputeCollectionPipeline(
    VkDevice const device,
    VkDescriptorSetLayout const drawImageDescriptorLayout,
    std::span<std::string const> const shaderPaths
)
{
    std::vector<VkDescriptorSetLayout> const layouts{drawImageDescriptorLayout};

    m_shaders.clear();
    for (std::string const& shaderPath : shaderPaths)
    {
        std::optional<ShaderObjectReflected> loadResult{
            vkutil::loadShaderObject(
                device, shaderPath, VK_SHADER_STAGE_COMPUTE_BIT, 0, layouts, {}
            )
        };

        if (!loadResult.has_value())
        {
            continue;
        }

        ShaderObjectReflected const shader{loadResult.value()};

        std::vector<VkPushConstantRange> ranges{};
        if (shader.reflectionData().defaultEntryPointHasPushConstant())
        {
            ShaderReflectionData::PushConstant const& pushConstant{
                shader.reflectionData().defaultPushConstant()
            };
            // For the buffer, we allocate extra bytes to the push constant
            // to simplify the offset math.
            // Host side, we write to a subset of this buffer, then only copy
            // the necessary range to the device.
            m_shaderPushConstants.emplace_back(
                pushConstant.type.paddedSizeBytes, 0
            );

            ranges.push_back(pushConstant.totalRange(VK_SHADER_STAGE_COMPUTE_BIT
            ));
        }
        else
        {
            m_shaderPushConstants.emplace_back();
        }

        m_shaders.push_back(shader);

        VkPipelineLayoutCreateInfo const layoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,

            .flags = 0,

            .setLayoutCount = 1,
            .pSetLayouts = &drawImageDescriptorLayout,

            .pushConstantRangeCount = static_cast<uint32_t>(ranges.size()),
            .pPushConstantRanges = ranges.data(),
        };

        VkPipelineLayout layout{VK_NULL_HANDLE};
        SZG_LOG_VK(
            vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &layout),
            "Creating shader object pipeline layout"
        );
        m_layouts.push_back(layout);
    }
}

void ComputeCollectionPipeline::recordDrawCommands(
    VkCommandBuffer const cmd,
    VkDescriptorSet const drawImageDescriptors,
    VkExtent2D const drawExtent
) const
{
    ShaderObjectReflected const& shader{currentShader()};

    VkShaderStageFlagBits const stage{VK_SHADER_STAGE_COMPUTE_BIT};
    VkShaderEXT const shaderObject{shader.shaderObject()};
    VkPipelineLayout const layout{currentLayout()};

    vkCmdBindShadersEXT(cmd, 1, &stage, &shaderObject);

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        layout,
        0,
        1,
        &drawImageDescriptors,
        0,
        nullptr
    );

    ShaderReflectionData const& reflectionData{shader.reflectionData()};
    if (reflectionData.defaultEntryPointHasPushConstant())
    {
        std::span<uint8_t const> const pushConstant{readPushConstantBytes()};
        std::vector<uint8_t> pushConstantBytes{
            pushConstant.begin(), pushConstant.end()
        };

        struct DrawRectPushConstantPrefix
        {
            glm::vec2 drawOffset{};
            glm::vec2 drawExtent{};
        };

        if (pushConstant.size() >= sizeof(DrawRectPushConstantPrefix))
        {
            // We assume the first two members of the push constant are
            // the offset and extent for rendering.
            // Both should be type vec2 in glsl

            auto* const pData{reinterpret_cast<DrawRectPushConstantPrefix*>(
                pushConstantBytes.data()
            )};

            *pData = DrawRectPushConstantPrefix{
                .drawOffset{glm::vec2{0.0}},
                .drawExtent{glm::vec2{drawExtent.width, drawExtent.height}},
            };
        }

        uint32_t const byteOffset{
            reflectionData.defaultPushConstant().layoutOffsetBytes
        };

        vkCmdPushConstants(
            cmd,
            layout,
            stage,
            byteOffset,
            pushConstantBytes.size() - byteOffset,
            pushConstantBytes.data() + byteOffset
        );
    }

    uint32_t constexpr WORKGROUP_SIZE{16};

    vkCmdDispatch(
        cmd,
        computeDispatchCount(drawExtent.width, WORKGROUP_SIZE),
        computeDispatchCount(drawExtent.height, WORKGROUP_SIZE),
        1
    );
}

void ComputeCollectionPipeline::cleanup(VkDevice const device)
{
    for (ShaderObjectReflected& shader : m_shaders)
    {
        shader.cleanup(device);
    }
    for (VkPipelineLayout const& layout : m_layouts)
    {
        vkDestroyPipelineLayout(device, layout, nullptr);
    }
}

DebugLineGraphicsPipeline::DebugLineGraphicsPipeline(
    VkDevice const device, ImageFormats const formats
)
{
    ShaderModuleReflected const vertexShader{
        vkutil::loadShaderModule(device, "shaders/debug/debugline.vert.spv")
            .value_or(ShaderModuleReflected::MakeInvalid())
    };
    ShaderModuleReflected const fragmentShader{
        vkutil::loadShaderModule(device, "shaders/debug/debugline.frag.spv")
            .value_or(ShaderModuleReflected::MakeInvalid())
    };

    std::vector<VkPushConstantRange> pushConstantRanges{};
    {
        // Vertex push constant
        ShaderReflectionData::PushConstant const& vertexPushConstant{
            vertexShader.reflectionData().defaultPushConstant()
        };

        size_t const vertexPushConstantSize{
            vertexPushConstant.type.paddedSizeBytes
            - vertexPushConstant.layoutOffsetBytes
        };
        size_t const vertexPushConstantSizeExpected{sizeof(VertexPushConstant)};

        if (vertexPushConstantSize != vertexPushConstantSizeExpected)
        {
            SZG_WARNING(fmt::format(
                "Loaded vertex push constant had "
                "a push constant of size {}, "
                "while implementation expects {}.",
                vertexPushConstantSize,
                vertexPushConstantSizeExpected
            ));
        }

        pushConstantRanges.push_back(
            vertexPushConstant.totalRange(VK_SHADER_STAGE_VERTEX_BIT)
        );
    }

    VkPipelineLayoutCreateInfo const layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,

        .setLayoutCount = 0,
        .pSetLayouts = nullptr,

        .pushConstantRangeCount =
            static_cast<uint32_t>(pushConstantRanges.size()),
        .pPushConstantRanges = pushConstantRanges.data(),
    };

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    SZG_CHECK_VK(
        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout)
    );

    PipelineBuilder pipelineBuilder{};
    pipelineBuilder.pushShader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    pipelineBuilder.pushShader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);
    pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.pushDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
    pipelineBuilder.setMultisamplingNone();
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_ALWAYS);

    pipelineBuilder.setColorAttachment(formats.color);
    pipelineBuilder.setDepthFormat(formats.depth);

    m_vertexShader = vertexShader;
    m_fragmentShader = fragmentShader;

    m_graphicsPipelineLayout = pipelineLayout;
    m_graphicsPipeline = pipelineBuilder.buildPipeline(device, pipelineLayout);
}

auto DebugLineGraphicsPipeline::recordDrawCommands(
    VkCommandBuffer const cmd,
    bool const reuseDepthAttachment,
    float const lineWidth,
    VkRect2D const drawRect,
    szg_image::ImageView& color,
    szg_image::ImageView& depth,
    uint32_t const cameraIndex,
    TStagedBuffer<gputypes::Camera> const& cameras,
    TStagedBuffer<Vertex> const& endpoints,
    TStagedBuffer<uint32_t> const& indices
) const -> DrawResultsGraphics
{
    VkRenderingAttachmentInfo const colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,

        .imageView = color.view(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,

        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    VkAttachmentLoadOp const depthLoadOp{
        reuseDepthAttachment ? VK_ATTACHMENT_LOAD_OP_LOAD
                             : VK_ATTACHMENT_LOAD_OP_CLEAR
    };
    VkRenderingAttachmentInfo const depthAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,

        .imageView = depth.view(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,

        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,

        .loadOp = depthLoadOp,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,

        .clearValue = VkClearValue{.depthStencil{.depth = 0.0F}},
    };

    std::vector<VkRenderingAttachmentInfo> const colorAttachments{
        colorAttachment
    };
    VkRenderingInfo const renderInfo{
        vkinit::renderingInfo(drawRect, colorAttachments, &depthAttachment)
    };

    cameras.recordTotalCopyBarrier(
        cmd,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT
    );
    endpoints.recordTotalCopyBarrier(
        cmd,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT
    );
    indices.recordTotalCopyBarrier(
        cmd, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT
    );

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    vkCmdSetLineWidth(cmd, lineWidth);

    VkViewport const viewport{
        .x = static_cast<float>(drawRect.offset.x),
        .y = static_cast<float>(drawRect.offset.y),
        .width = static_cast<float>(drawRect.extent.width),
        .height = static_cast<float>(drawRect.extent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F,
    };

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D const scissor{drawRect};

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    { // Vertex push constant
        VertexPushConstant const vertexPushConstant{
            .vertexBuffer = endpoints.deviceAddress(),
            .cameraBuffer = cameras.deviceAddress(),
            .cameraIndex = cameraIndex,
        };
        vkCmdPushConstants(
            cmd,
            m_graphicsPipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(VertexPushConstant),
            &vertexPushConstant
        );
        m_vertexPushConstant = vertexPushConstant;
    }

    // Bind the entire index buffer of the mesh,
    // but only draw a single surface.
    vkCmdBindIndexBuffer(cmd, indices.deviceBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDraw(cmd, indices.deviceSize(), 1, 0, 0);

    vkCmdEndRendering(cmd);

    return DrawResultsGraphics{
        .drawCalls = 1,
        .verticesDrawn = endpoints.deviceSize(),
        .indicesDrawn = indices.deviceSize(),
    };
}

void DebugLineGraphicsPipeline::cleanup(VkDevice const device)
{
    m_fragmentShader.cleanup(device);
    m_vertexShader.cleanup(device);

    vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, m_graphicsPipelineLayout, nullptr);
}

OffscreenPassGraphicsPipeline::OffscreenPassGraphicsPipeline(
    VkDevice const device, VkFormat const depthAttachmentFormat
)
{
    ShaderModuleReflected const vertexShader{
        vkutil::loadShaderModule(
            device, "shaders/offscreenpass/depthpass.vert.spv"
        )
            .value_or(ShaderModuleReflected::MakeInvalid())
    };

    std::vector<VkPushConstantRange> pushConstantRanges{};
    {
        // Vertex push constant
        ShaderReflectionData::PushConstant const& vertexPushConstant{
            vertexShader.reflectionData().defaultPushConstant()
        };

        size_t const vertexPushConstantSize{
            vertexPushConstant.type.paddedSizeBytes
            - vertexPushConstant.layoutOffsetBytes
        };
        size_t const vertexPushConstantSizeExpected{sizeof(VertexPushConstant)};

        if (vertexPushConstantSize != vertexPushConstantSizeExpected)
        {
            SZG_WARNING(fmt::format(
                "Loaded vertex push constant had "
                "a push constant of size {}, "
                "while implementation expects {}.",
                vertexPushConstantSize,
                vertexPushConstantSizeExpected
            ));
        }

        pushConstantRanges.push_back(
            vertexPushConstant.totalRange(VK_SHADER_STAGE_VERTEX_BIT)
        );
    }

    VkPipelineLayoutCreateInfo const layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,

        .setLayoutCount = 0,
        .pSetLayouts = nullptr,

        .pushConstantRangeCount =
            static_cast<uint32_t>(pushConstantRanges.size()),
        .pPushConstantRanges = pushConstantRanges.data(),
    };

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    SZG_CHECK_VK(
        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout)
    );

    PipelineBuilder pipelineBuilder{};
    pipelineBuilder.pushShader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    // NO fragment shader

    pipelineBuilder.pushDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
    pipelineBuilder.enableDepthBias();

    pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.setCullMode(
        VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE
    );
    pipelineBuilder.setMultisamplingNone();
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // NO color attachment, just depth
    pipelineBuilder.setDepthFormat(depthAttachmentFormat);

    m_vertexShader = vertexShader;

    m_graphicsPipelineLayout = pipelineLayout;
    m_graphicsPipeline = pipelineBuilder.buildPipeline(device, pipelineLayout);
}

void OffscreenPassGraphicsPipeline::recordDrawCommands(
    VkCommandBuffer const cmd,
    bool const reuseDepthAttachment,
    float const depthBias,
    float const depthBiasSlope,
    szg_image::ImageView& depth,
    uint32_t const projViewIndex,
    TStagedBuffer<glm::mat4x4> const& projViewMatrices,
    std::span<scene::MeshInstanced const> const geometry,
    std::span<RenderOverride const> const renderOverrides
) const
{
    VkAttachmentLoadOp const depthLoadOp{
        reuseDepthAttachment ? VK_ATTACHMENT_LOAD_OP_LOAD
                             : VK_ATTACHMENT_LOAD_OP_CLEAR
    };
    VkRenderingAttachmentInfo const depthAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,

        .imageView = depth.view(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,

        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,

        .loadOp = depthLoadOp,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,

        .clearValue = VkClearValue{.depthStencil{.depth = 0.0F}},
    };

    VkExtent2D const depthExtent{depth.image().extent2D()};

    VkRenderingInfo const renderInfo{vkinit::renderingInfo(
        VkRect2D{.extent = depthExtent}, {}, &depthAttachment
    )};

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    vkCmdSetDepthBias(cmd, depthBias, 0.0, depthBiasSlope);

    VkViewport const viewport{
        .x = 0,
        .y = 0,
        .width = static_cast<float>(depthExtent.width),
        .height = static_cast<float>(depthExtent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F,
    };

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D const scissor{
        .offset{
            .x = 0,
            .y = 0,
        },
        .extent{depthExtent},
    };

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    for (size_t index{0}; index < geometry.size(); index++)
    {
        scene::MeshInstanced const& instance{geometry[index]};

        bool render{instance.render};
        if (index < renderOverrides.size())
        {
            RenderOverride const& renderOverride{renderOverrides[index]};

            render = renderOverride.render;
        }

        if (!render)
        {
            continue;
        }

        MeshAsset const& meshAsset{*instance.mesh};
        TStagedBuffer<glm::mat4x4> const& models{*instance.models};

        GPUMeshBuffers& meshBuffers{*meshAsset.meshBuffers};

        { // Vertex push constant
            VertexPushConstant const vertexPushConstant{
                .vertexBufferAddress = meshBuffers.vertexAddress(),
                .modelBufferAddress = models.deviceAddress(),
                .projViewBufferAddress = projViewMatrices.deviceAddress(),
                .projViewIndex = projViewIndex,
            };
            vkCmdPushConstants(
                cmd,
                m_graphicsPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(VertexPushConstant),
                &vertexPushConstant
            );
        }

        GeometrySurface const& drawnSurface{meshAsset.surfaces[0]};

        // Bind the entire index buffer of the mesh,
        // but only draw a single surface.
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

    vkCmdEndRendering(cmd);
}

void OffscreenPassGraphicsPipeline::cleanup(VkDevice const device)
{
    m_vertexShader.cleanup(device);

    vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, m_graphicsPipelineLayout, nullptr);
}
