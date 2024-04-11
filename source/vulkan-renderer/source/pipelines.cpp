#include "pipelines.hpp"

#include <fstream>
#include "helpers.h"
#include "initializers.hpp"
#include "shaders.hpp"

ShaderWrapper vkutil::loadShaderModule(std::string const& localPath, VkDevice device)
{
	Log(fmt::format("Compiling \"{}\"", localPath));
	std::unique_ptr<std::filesystem::path> const pShaderPath = DebugUtils::getLoadedDebugUtils().loadAssetPath(std::filesystem::path(localPath));
	if (pShaderPath == nullptr)
	{
		Error(fmt::format("Unable to get asset at \"{}\"", localPath));
		return ShaderWrapper::Invalid();
	}

	std::filesystem::path const shaderPath = *pShaderPath.get();
	
	std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		Error(fmt::format("Unable to open shader at \"{}\"", localPath));
		return ShaderWrapper::Invalid();
	}

	size_t const fileSizeBytes = static_cast<size_t>(file.tellg());
	if (fileSizeBytes == 0)
	{
		Error(fmt::format("Shader file is empty at \"{}\"", localPath));
		return ShaderWrapper::Invalid();
	}

	std::vector<uint8_t> buffer(fileSizeBytes);

	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char*>(buffer.data()), fileSizeBytes);

	file.close();

	std::string const shaderName = shaderPath.filename().string();
	return ShaderWrapper::FromBytecode(device, shaderName, buffer);
}

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkPipelineLayout layout) const
{
	VkPipelineViewportStateCreateInfo const viewportState{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO },
		.pNext{ nullptr },

		.viewportCount{ 1 },
		.scissorCount{ 1 },

		// We use dynamic rendering, so no other members are needed
	};

	VkPipelineColorBlendStateCreateInfo const colorBlending{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO },
		.pNext{ nullptr },

		.logicOpEnable{ VK_FALSE },
		.logicOp{ VK_LOGIC_OP_COPY },

		.attachmentCount{ 1 },
		.pAttachments{ &m_colorBlendAttachment },
	};

	// Dummy vertex input 
	VkPipelineVertexInputStateCreateInfo const vertexInputInfo
	{ 
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO }
	};

	std::array<VkDynamicState, 2> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo const dynamicInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO },

		.dynamicStateCount{ static_cast<uint32_t>(dynamicStates.size()) },
		.pDynamicStates{ dynamicStates.data() }
	};

	VkPipelineRenderingCreateInfo renderInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO },
		.pNext{ nullptr },

		.colorAttachmentCount{ 1 },
		.pColorAttachmentFormats{ &m_colorAttachmentFormat },

		.depthAttachmentFormat{ m_depthAttachmentFormat },
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
	CheckVkResult(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
	return pipeline;
}

void PipelineBuilder::setShaders(ShaderWrapper const& vertexShader, ShaderWrapper const& fragmentShader)
{
	m_shaderStages.clear();
	m_shaderStages.push_back(vkinit::pipelineShaderStageCreateInfo(
		VK_SHADER_STAGE_VERTEX_BIT,
		vertexShader.shaderModule(),
		vertexShader.reflectionData().defaultEntryPoint
	));
	m_shaderStages.push_back(vkinit::pipelineShaderStageCreateInfo(
		VK_SHADER_STAGE_FRAGMENT_BIT,
		fragmentShader.shaderModule(),
		fragmentShader.reflectionData().defaultEntryPoint
	));
}

void PipelineBuilder::setInputTopology(VkPrimitiveTopology topology)
{
	m_inputAssembly.topology = topology;
	m_inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::setPolygonMode(VkPolygonMode mode)
{
	m_rasterizer.polygonMode = mode;
	m_rasterizer.lineWidth = 1.0f;
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

void PipelineBuilder::disableBlending()
{
	m_colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;

	m_colorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::setColorAttachmentFormat(VkFormat format)
{
	m_colorAttachmentFormat = format;
}

void PipelineBuilder::setDepthFormat(VkFormat format)
{
	m_depthAttachmentFormat = format;
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

InstancedMeshGraphicsPipeline::InstancedMeshGraphicsPipeline(
	VkDevice device,
	VkFormat colorAttachmentFormat,
	VkFormat depthAttachmentFormat
)
{
	ShaderWrapper const vertexShader{ vkutil::loadShaderModule("shaders/instanced_mesh.vert.spv", device) };
	ShaderWrapper const fragmentShader{ vkutil::loadShaderModule("shaders/instanced_mesh.frag.spv", device) };

	ShaderReflectionData::PushConstant const& vertexPushConstant{ vertexShader.reflectionData().defaultPushConstant() };

	assert(vertexPushConstant.type.paddedSizeBytes == sizeof(PushConstantType));

	VkPushConstantRange const pushConstantRange{
		.stageFlags{ VK_SHADER_STAGE_VERTEX_BIT },
		.offset{ 0 },
		.size{ sizeof(PushConstantType) },
	};

	VkPipelineLayoutCreateInfo const layoutInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO },
		.pNext{ nullptr },

		.flags{ 0 },

		.setLayoutCount{ 0 },
		.pSetLayouts{ nullptr },

		.pushConstantRangeCount{ 1 },
		.pPushConstantRanges{ &pushConstantRange },
	};

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	CheckVkResult(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

	PipelineBuilder pipelineBuilder{};
	pipelineBuilder.setShaders(vertexShader, fragmentShader);
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.setMultisamplingNone();
	pipelineBuilder.disableBlending();
	pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	pipelineBuilder.setColorAttachmentFormat(colorAttachmentFormat);
	pipelineBuilder.setDepthFormat(depthAttachmentFormat);


	m_vertexShader = vertexShader;
	m_fragmentShader = fragmentShader;

	m_graphicsPipelineLayout = pipelineLayout;
	m_graphicsPipeline = pipelineBuilder.buildPipeline(device, pipelineLayout);
}

void InstancedMeshGraphicsPipeline::recordDrawCommands(
	VkCommandBuffer cmd,
	glm::mat4x4 camera,
	AllocatedImage const& color,
	AllocatedImage const& depth,
	MeshAsset const& mesh,
	TStagedBuffer<glm::mat4x4> const& transforms
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
	VkRenderingAttachmentInfo const depthAttachment{
		.sType{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO },
		.pNext{ nullptr },

		.imageView{ depth.imageView },
		.imageLayout{ VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL },

		.resolveMode{ VK_RESOLVE_MODE_NONE },
		.resolveImageView{ VK_NULL_HANDLE },
		.resolveImageLayout{ VK_IMAGE_LAYOUT_UNDEFINED },

		.loadOp{ VK_ATTACHMENT_LOAD_OP_CLEAR },
		.storeOp{ VK_ATTACHMENT_STORE_OP_STORE },

		.clearValue{ VkClearValue{.depthStencil{.depth{ 0.0f }}} },
	};

	VkExtent2D const drawExtent{
		.width{color.imageExtent.width},
		.height{color.imageExtent.height},
	};
	std::vector<VkRenderingAttachmentInfo> const colorAttachments{ colorAttachment };
	VkRenderingInfo const renderInfo{
		vkinit::renderingInfo(drawExtent, colorAttachments, &depthAttachment)
	};

	if (transforms.deviceSize() > 0)
	{
		VkMemoryBarrier2 const transformBarrier{
			.sType{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 },
			.pNext{ nullptr },

			.srcStageMask{ VK_PIPELINE_STAGE_2_COPY_BIT },
			.srcAccessMask{ VK_ACCESS_TRANSFER_WRITE_BIT },

			.dstStageMask{ VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT },
			.dstAccessMask{ VK_ACCESS_2_SHADER_STORAGE_READ_BIT },
		};

		VkBufferMemoryBarrier2 const bufferMemoryBarrier{
			.sType{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 },
			.pNext{ nullptr },

			.srcStageMask{ VK_PIPELINE_STAGE_2_COPY_BIT },
			.srcAccessMask{ VK_ACCESS_2_TRANSFER_WRITE_BIT },

			.dstStageMask{ VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT },
			.dstAccessMask{ VK_ACCESS_2_SHADER_STORAGE_READ_BIT },

			.srcQueueFamilyIndex{ VK_QUEUE_FAMILY_IGNORED },
			.dstQueueFamilyIndex{ VK_QUEUE_FAMILY_IGNORED },

			.buffer{ transforms.deviceBuffer() },
			.offset{ 0 },
			.size{ transforms.deviceSizeBytes() },
		};

		VkDependencyInfo const transformsDependency{
			.sType{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO },
			.pNext{ nullptr },

			.dependencyFlags{ 0 },

			.memoryBarrierCount{ 0 },
			.pMemoryBarriers{ nullptr },

			.bufferMemoryBarrierCount{ 1 },
			.pBufferMemoryBarriers{ &bufferMemoryBarrier },

			.imageMemoryBarrierCount{ 0 },
			.pImageMemoryBarriers{ nullptr },
		};

		vkCmdPipelineBarrier2(cmd, &transformsDependency);
	}

	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

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

	VkBuffer const indexBuffer{ meshBuffers.indexBuffer() };
	VkDeviceAddress const transformsAddress{ transforms.address() };
	VkDeviceAddress const verticesAddress{ meshBuffers.vertexAddress() };

	assert(transformsAddress != 0);
	assert(verticesAddress != 0);
	PushConstantType const pushConstant{
		.cameraTransform{ camera },
		.vertexBufferAddress{ verticesAddress },
		.transformBufferAddress{ transformsAddress }
	};

	vkCmdPushConstants(cmd, m_graphicsPipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT,
		0, sizeof(PushConstantType), &pushConstant
	);

	GeometrySurface const& drawnSurface{ mesh.surfaces[0] };

	// Bind the entire index buffer of the mesh, but only draw a single surface.
	vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, drawnSurface.indexCount, transforms.deviceSize(), drawnSurface.firstIndex, 0, 0);

	vkCmdEndRendering(cmd);
}

void InstancedMeshGraphicsPipeline::cleanup(VkDevice device)
{
	m_vertexShader.cleanup(device);
	m_fragmentShader.cleanup(device);

	vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, m_graphicsPipelineLayout, nullptr);
}
