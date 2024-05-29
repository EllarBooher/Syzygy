#include "pipelines.hpp"

#include <fstream>
#include "helpers.hpp"
#include "initializers.hpp"
#include "shaders.hpp"

#include <glm/gtx/intersect.hpp>

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkPipelineLayout layout) const
{
	VkPipelineViewportStateCreateInfo const viewportState{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO },
		.pNext{ nullptr },

		.viewportCount{ 1 },
		.scissorCount{ 1 },

		// We use dynamic rendering, so no other members are needed
	};

	std::vector<VkFormat> colorFormats{};
	std::vector<VkPipelineColorBlendAttachmentState> attachmentStates{};
	if (m_colorAttachment.has_value())
	{
		ColorAttachmentSpecification const specification{ m_colorAttachment.value() };

		colorFormats.push_back(specification.format);
		attachmentStates.push_back(specification.blending);
	}

	VkPipelineColorBlendStateCreateInfo const colorBlending{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO },
		.pNext{ nullptr },

		.logicOpEnable{ VK_FALSE },
		.logicOp{ VK_LOGIC_OP_COPY },

		.attachmentCount{ static_cast<uint32_t>(attachmentStates.size()) },
		.pAttachments{ attachmentStates.data() },
	};

	VkPipelineRenderingCreateInfo const renderInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO },
		.pNext{ nullptr },

		.colorAttachmentCount{ static_cast<uint32_t>(colorFormats.size()) },
		.pColorAttachmentFormats{ colorFormats.data() },

		.depthAttachmentFormat{ m_depthAttachmentFormat },
	};

	// Dummy vertex input 
	VkPipelineVertexInputStateCreateInfo const vertexInputInfo
	{ 
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO }
	};

	std::vector<VkDynamicState> dynamicStates{m_dynamicStates.begin(), m_dynamicStates.end()};

	// We insert these by default since we have no methods for setting the static state for now
	dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

	VkPipelineDynamicStateCreateInfo const dynamicInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO },

		.dynamicStateCount{ static_cast<uint32_t>(dynamicStates.size()) },
		.pDynamicStates{ dynamicStates.data() }
	};


	VkGraphicsPipelineCreateInfo const pipelineInfo{
		.sType{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO },
		.pNext{ &renderInfo },

		.stageCount{ static_cast<uint32_t>(m_shaderStages.size()) },
		.pStages{ m_shaderStages.data() },

		.pVertexInputState{ &vertexInputInfo },
		.pInputAssemblyState{ &m_inputAssembly },

		.pTessellationState{ nullptr },

		.pViewportState{ &viewportState },
		.pRasterizationState{ &m_rasterizer },
		.pMultisampleState{ &m_multisampling },

		.pDepthStencilState{ &m_depthStencil },
		.pColorBlendState{ &colorBlending },
		.pDynamicState{ &dynamicInfo },

		.layout{ layout },
		.renderPass{ VK_NULL_HANDLE }, // dynamic rendering
		.subpass{ 0 },
		.basePipelineHandle{ VK_NULL_HANDLE },
		.basePipelineIndex{ 0 },
	};

	VkPipeline pipeline{ VK_NULL_HANDLE };
	LogVkResult(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "Building graphics pipeline");
	return pipeline;
}

void PipelineBuilder::pushShader(ShaderModuleReflected const& shader, VkShaderStageFlagBits stage)
{
	m_shaderStages.push_back(
		vkinit::pipelineShaderStageCreateInfo(
			stage
			, shader.shaderModule()
			, shader.reflectionData().defaultEntryPoint
		)
	);
}

void PipelineBuilder::setInputTopology(VkPrimitiveTopology topology)
{
	m_inputAssembly.topology = topology;
	m_inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::setPolygonMode(VkPolygonMode mode)
{
	m_rasterizer.polygonMode = mode;
}

void PipelineBuilder::pushDynamicState(VkDynamicState dynamicState)
{
	m_dynamicStates.insert(dynamicState);
}

void PipelineBuilder::setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	m_rasterizer.cullMode = cullMode;
	m_rasterizer.frontFace = frontFace;
}

void PipelineBuilder::setMultisamplingNone()
{
	m_multisampling.sampleShadingEnable = VK_FALSE;
	m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_multisampling.minSampleShading = 1.0f;
	m_multisampling.pSampleMask = nullptr;
	m_multisampling.alphaToCoverageEnable = VK_FALSE;
	m_multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::setColorAttachment(VkFormat format)
{
	m_colorAttachment = ColorAttachmentSpecification{
		.format{ format },
	};
}

void PipelineBuilder::setDepthFormat(VkFormat format)
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
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO },
		.pNext{ nullptr },

		.flags{ 0 },
		.depthTestEnable{ VK_FALSE },
		.depthWriteEnable{ VK_FALSE },
		.depthCompareOp{ VK_COMPARE_OP_NEVER },
		.depthBoundsTestEnable{ VK_FALSE },
		.stencilTestEnable{ VK_FALSE },
		.front{},
		.back{},
		.minDepthBounds{ 0.0f },
		.maxDepthBounds{ 1.0f },
	};
}

void PipelineBuilder::enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp)
{
	m_depthStencil = VkPipelineDepthStencilStateCreateInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO },
		.pNext{ nullptr },

		.flags{ 0 },
		.depthTestEnable{ VK_TRUE },
		.depthWriteEnable{ depthWriteEnable ? VK_TRUE : VK_FALSE },
		.depthCompareOp{ compareOp },
		.depthBoundsTestEnable{ VK_FALSE },
		.stencilTestEnable{ VK_FALSE },
		.front{},
		.back{},
		.minDepthBounds{ 0.0f },
		.maxDepthBounds{ 1.0f },
	};
}

GenericComputeCollectionPipeline::GenericComputeCollectionPipeline(
	VkDevice device
	, VkDescriptorSetLayout drawImageDescriptorLayout
	, std::span<std::string const> shaderPaths
)
{
	std::vector<VkDescriptorSetLayout> const layouts{ drawImageDescriptorLayout };

	m_shaders.clear();
	for (std::string const& shaderPath : shaderPaths)
	{
		std::optional<ShaderObjectReflected> loadResult{ 
			vkutil::loadShaderObject(
				device
				, shaderPath
				, VK_SHADER_STAGE_COMPUTE_BIT
				, 0
				, layouts
				, {}
			) 
		};

		if (!loadResult.has_value()) { continue; }

		ShaderObjectReflected const shader{ loadResult.value() };

		std::vector<VkPushConstantRange> ranges{};
		if (shader.reflectionData().defaultEntryPointHasPushConstant())
		{
			ShaderReflectionData::PushConstant const& pushConstant{ shader.reflectionData().defaultPushConstant() };
			// For the buffer, we allocate extra bytes to the push constant to simplify the offset math.
			// Host side, we write to a subset of this buffer, then only copy the necessary range to the device.
			m_shaderPushConstants.push_back(
				std::vector<uint8_t>(pushConstant.type.paddedSizeBytes, 0)
			);

			ranges.push_back(pushConstant.totalRange(VK_SHADER_STAGE_COMPUTE_BIT));
		}
		else
		{
			m_shaderPushConstants.push_back({});
		}

		m_shaders.push_back(
			shader
		);

		VkPipelineLayoutCreateInfo const layoutCreateInfo{
			.sType{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO },
			.pNext{ nullptr },

			.flags{ 0 },

			.setLayoutCount{ 1 },
			.pSetLayouts{ &drawImageDescriptorLayout },

			.pushConstantRangeCount{ static_cast<uint32_t>(ranges.size()) },
			.pPushConstantRanges{ ranges.data() },
		};

		VkPipelineLayout layout{ VK_NULL_HANDLE };
		LogVkResult(vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &layout), "Creating shader object pipeline layout");
		m_layouts.push_back(layout);
	}
}

void GenericComputeCollectionPipeline::recordDrawCommands(
	VkCommandBuffer cmd
	, VkDescriptorSet drawImageDescriptors
	, VkExtent2D drawExtent
) const
{
	ShaderObjectReflected const& shader{ currentShader() };

	VkShaderStageFlagBits const stage{ VK_SHADER_STAGE_COMPUTE_BIT };
	VkShaderEXT const shaderObject{ shader.shaderObject() };
	VkPipelineLayout const layout{ currentLayout() };

	vkCmdBindShadersEXT(cmd, 1, &stage, &shaderObject);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &drawImageDescriptors, 0, nullptr);

	ShaderReflectionData const& reflectionData{ shader.reflectionData() };
	if (reflectionData.defaultEntryPointHasPushConstant())
	{
		std::span<uint8_t const> const pushConstant{ readPushConstantBytes() };
		std::vector<uint8_t> pushConstantBytes{ pushConstant.begin(), pushConstant.end() };

		if (pushConstant.size() >= 16)
		{
			// We assume the first two members of the push constant are the offset and extent for rendering.
			// Both should be type vec2 in glsl
			struct DrawRectPushConstant
			{
				glm::vec2 drawOffset{};
				glm::vec2 drawExtent{};
			};

			*reinterpret_cast<DrawRectPushConstant*>(pushConstantBytes.data()) = DrawRectPushConstant{
				.drawOffset{ glm::vec2{0.0} },
				.drawExtent{ glm::vec2{drawExtent.width, drawExtent.height} },
			};
		}

		uint32_t const byteOffset{ reflectionData.defaultPushConstant().layoutOffsetBytes };

		vkCmdPushConstants(cmd, layout, stage, byteOffset, pushConstantBytes.size() - byteOffset, pushConstantBytes.data() + byteOffset);
	}

	vkCmdDispatch(cmd, std::ceil(drawExtent.width / 16.0), std::ceil(drawExtent.height / 16.0), 1);
}

void GenericComputeCollectionPipeline::cleanup(VkDevice device)
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

DebugLineComputePipeline::DebugLineComputePipeline(
	VkDevice device
	, VkFormat colorAttachmentFormat
	, VkFormat depthAttachmentFormat
)
{
	ShaderModuleReflected const vertexShader{ vkutil::loadShaderModule(device, "shaders/debug/debugline.vert.spv").value() };
	ShaderModuleReflected const fragmentShader{ vkutil::loadShaderModule(device, "shaders/debug/debugline.frag.spv").value() };

	std::vector<VkPushConstantRange> pushConstantRanges{};
	{
		// Vertex push constant
		ShaderReflectionData::PushConstant const& vertexPushConstant{ vertexShader.reflectionData().defaultPushConstant() };

		size_t const vertexPushConstantSize{ vertexPushConstant.type.paddedSizeBytes - vertexPushConstant.layoutOffsetBytes };
		size_t const vertexPushConstantSizeExpected{ sizeof(VertexPushConstant) };

		if (vertexPushConstantSize != vertexPushConstantSizeExpected) {
			Warning(fmt::format("Loaded vertex push constant had a push constant of size {}, while implementation expects {}."
				, vertexPushConstantSize
				, vertexPushConstantSizeExpected
			));
		}

		pushConstantRanges.push_back(vertexPushConstant.totalRange(VK_SHADER_STAGE_VERTEX_BIT));
	}

	VkPipelineLayoutCreateInfo const layoutInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO },
		.pNext{ nullptr },

		.flags{ 0 },

		.setLayoutCount{ 0 },
		.pSetLayouts{ nullptr },

		.pushConstantRangeCount{ static_cast<uint32_t>(pushConstantRanges.size()) },
		.pPushConstantRanges{ pushConstantRanges.data() },
	};

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	CheckVkResult(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

	PipelineBuilder pipelineBuilder{};
	pipelineBuilder.pushShader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
	pipelineBuilder.pushShader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.pushDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
	pipelineBuilder.setMultisamplingNone();
	pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_ALWAYS);

	pipelineBuilder.setColorAttachment(colorAttachmentFormat);
	pipelineBuilder.setDepthFormat(depthAttachmentFormat);

	m_vertexShader = vertexShader;
	m_fragmentShader = fragmentShader;

	m_graphicsPipelineLayout = pipelineLayout;
	m_graphicsPipeline = pipelineBuilder.buildPipeline(device, pipelineLayout);
}

DrawResultsGraphics DebugLineComputePipeline::recordDrawCommands(
	VkCommandBuffer cmd
	, bool reuseDepthAttachment
	, float lineWidth
	, VkRect2D drawRect
	, AllocatedImage const& color
	, AllocatedImage const& depth
	, uint32_t cameraIndex
	, TStagedBuffer<GPUTypes::Camera> const& cameras
	, TStagedBuffer<Vertex> const& endpoints
	, TStagedBuffer<uint32_t> const& indices
) const
{
	VkRenderingAttachmentInfo const colorAttachment{
		.sType{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO },
		.pNext{ nullptr },

		.imageView{ color.imageView },
		.imageLayout{ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },

		.resolveMode{ VK_RESOLVE_MODE_NONE },
		.resolveImageView{ VK_NULL_HANDLE },
		.resolveImageLayout{ VK_IMAGE_LAYOUT_UNDEFINED },

		.loadOp{ VK_ATTACHMENT_LOAD_OP_LOAD },
		.storeOp{ VK_ATTACHMENT_STORE_OP_STORE },

		.clearValue{},
	};
	VkAttachmentLoadOp const depthLoadOp{ reuseDepthAttachment
		? VK_ATTACHMENT_LOAD_OP_LOAD
		: VK_ATTACHMENT_LOAD_OP_CLEAR
	};
	VkRenderingAttachmentInfo const depthAttachment{
		.sType{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO },
		.pNext{ nullptr },

		.imageView{ depth.imageView },
		.imageLayout{ VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL },

		.resolveMode{ VK_RESOLVE_MODE_NONE },
		.resolveImageView{ VK_NULL_HANDLE },
		.resolveImageLayout{ VK_IMAGE_LAYOUT_UNDEFINED },

		.loadOp{ depthLoadOp },
		.storeOp{ VK_ATTACHMENT_STORE_OP_STORE },

		.clearValue{ VkClearValue{.depthStencil{.depth{ 0.0f }}} },
	};

	std::vector<VkRenderingAttachmentInfo> const colorAttachments{ colorAttachment };
	VkRenderingInfo const renderInfo{
		vkinit::renderingInfo(drawRect, colorAttachments, &depthAttachment)
	};

	cameras.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
	endpoints.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
	indices.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT);

	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	vkCmdSetLineWidth(cmd, lineWidth);

	VkViewport const viewport{
		.x{ static_cast<float>(drawRect.offset.x) },
		.y{ static_cast<float>(drawRect.offset.y) },
		.width{ static_cast<float>(drawRect.extent.width) },
		.height{ static_cast<float>(drawRect.extent.height) },
		.minDepth{ 0.0f },
		.maxDepth{ 1.0f },
	};

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D const scissor{ drawRect };

	vkCmdSetScissor(cmd, 0, 1, &scissor);

	{ // Vertex push constant
		VertexPushConstant const vertexPushConstant{
			.vertexBuffer{ endpoints.deviceAddress() },
			.cameraBuffer{ cameras.deviceAddress() },
			.cameraIndex{ cameraIndex },
		};
		vkCmdPushConstants(cmd, m_graphicsPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(VertexPushConstant), &vertexPushConstant
		);
		m_vertexPushConstant = vertexPushConstant;
	}

	// Bind the entire index buffer of the mesh, but only draw a single surface.
	vkCmdBindIndexBuffer(cmd, indices.deviceBuffer(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDraw(cmd, indices.deviceSize(), 1, 0, 0);

	vkCmdEndRendering(cmd);

	return DrawResultsGraphics{
		.drawCalls{ 1 },
		.verticesDrawn{ endpoints.deviceSize() },
		.indicesDrawn{ indices.deviceSize() }
	};
}

void DebugLineComputePipeline::cleanup(VkDevice device)
{
	m_fragmentShader.cleanup(device);
	m_vertexShader.cleanup(device);

	vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, m_graphicsPipelineLayout, nullptr);
}

OffscreenPassInstancedMeshGraphicsPipeline::OffscreenPassInstancedMeshGraphicsPipeline(VkDevice device, VkFormat depthAttachmentFormat)
{
	ShaderModuleReflected const vertexShader{ vkutil::loadShaderModule(device, "shaders/offscreenpass/depthpass.vert.spv").value() };

	std::vector<VkPushConstantRange> pushConstantRanges{};
	{
		// Vertex push constant
		ShaderReflectionData::PushConstant const& vertexPushConstant{ vertexShader.reflectionData().defaultPushConstant() };

		size_t const vertexPushConstantSize{ vertexPushConstant.type.paddedSizeBytes - vertexPushConstant.layoutOffsetBytes };
		size_t const vertexPushConstantSizeExpected{ sizeof(VertexPushConstant) };

		if (vertexPushConstantSize != vertexPushConstantSizeExpected) {
			Warning(fmt::format("Loaded vertex push constant had a push constant of size {}, while implementation expects {}."
				, vertexPushConstantSize
				, vertexPushConstantSizeExpected
			));
		}

		pushConstantRanges.push_back(vertexPushConstant.totalRange(VK_SHADER_STAGE_VERTEX_BIT));
	}

	VkPipelineLayoutCreateInfo const layoutInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO },
		.pNext{ nullptr },

		.flags{ 0 },

		.setLayoutCount{ 0 },
		.pSetLayouts{ nullptr },

		.pushConstantRangeCount{ static_cast<uint32_t>(pushConstantRanges.size()) },
		.pPushConstantRanges{ pushConstantRanges.data() },
	};

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	CheckVkResult(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

	PipelineBuilder pipelineBuilder{};
	pipelineBuilder.pushShader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
	// NO fragment shader

	pipelineBuilder.pushDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
	pipelineBuilder.enableDepthBias();

	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.setCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.setMultisamplingNone();
	pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	// NO color attachment, just depth
	pipelineBuilder.setDepthFormat(depthAttachmentFormat);

	m_vertexShader = vertexShader;

	m_graphicsPipelineLayout = pipelineLayout;
	m_graphicsPipeline = pipelineBuilder.buildPipeline(device, pipelineLayout);

}

void OffscreenPassInstancedMeshGraphicsPipeline::recordDrawCommands(
	VkCommandBuffer cmd
	, bool reuseDepthAttachment
	, float depthBias
	, float depthBiasSlope
	, AllocatedImage const& depth
	, uint32_t projViewIndex
	, TStagedBuffer<glm::mat4x4> const& projViewMatrices
	, MeshAsset const& mesh
	, TStagedBuffer<glm::mat4x4> const& models
) const
{
	VkAttachmentLoadOp const depthLoadOp{ reuseDepthAttachment
		? VK_ATTACHMENT_LOAD_OP_LOAD
		: VK_ATTACHMENT_LOAD_OP_CLEAR
	};
	VkRenderingAttachmentInfo const depthAttachment{
		.sType{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO },
		.pNext{ nullptr },

		.imageView{ depth.imageView },
		.imageLayout{ VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL },

		.resolveMode{ VK_RESOLVE_MODE_NONE },
		.resolveImageView{ VK_NULL_HANDLE },
		.resolveImageLayout{ VK_IMAGE_LAYOUT_UNDEFINED },

		.loadOp{ depthLoadOp },
		.storeOp{ VK_ATTACHMENT_STORE_OP_STORE },

		.clearValue{ VkClearValue{.depthStencil{.depth{ 0.0f }}} },
	};

	VkExtent2D const drawExtent{
		.width{depth.imageExtent.width},
		.height{depth.imageExtent.height},
	};
	VkRenderingInfo const renderInfo{
		vkinit::renderingInfo(VkRect2D{.extent{drawExtent}}, {}, &depthAttachment)
	};

	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	vkCmdSetDepthBias(cmd, depthBias, 0.0, depthBiasSlope);

	VkViewport const viewport{
		.x{ 0 },
		.y{ 0 },
		.width{ static_cast<float>(drawExtent.width) },
		.height{ static_cast<float>(drawExtent.height) },
		.minDepth{ 0.0f },
		.maxDepth{ 1.0f },
	};

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D const scissor{
		.offset{
			.x{ 0 },
			.y{ 0 },
		},
		.extent{
			.width{ drawExtent.width },
			.height{ drawExtent.height },
		},
	};

	vkCmdSetScissor(cmd, 0, 1, &scissor);


	GPUMeshBuffers& meshBuffers{ *mesh.meshBuffers };

	{ // Vertex push constant
		VertexPushConstant const vertexPushConstant{
			.vertexBufferAddress{ meshBuffers.vertexAddress() },
			.modelBufferAddress{ models.deviceAddress() },
			.projViewBufferAddress{ projViewMatrices.deviceAddress() },
			.projViewIndex{ projViewIndex }
		};
		vkCmdPushConstants(cmd, m_graphicsPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(VertexPushConstant), &vertexPushConstant
		);
		m_vertexPushConstant = vertexPushConstant;
	}

	GeometrySurface const& drawnSurface{ mesh.surfaces[0] };

	// Bind the entire index buffer of the mesh, but only draw a single surface.
	vkCmdBindIndexBuffer(cmd, meshBuffers.indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, drawnSurface.indexCount, models.deviceSize(), drawnSurface.firstIndex, 0, 0);

	vkCmdEndRendering(cmd);
}

void OffscreenPassInstancedMeshGraphicsPipeline::cleanup(VkDevice device)
{
	m_vertexShader.cleanup(device);

	vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, m_graphicsPipelineLayout, nullptr);
}
