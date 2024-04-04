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
		.front{},
		.back{},
		.minDepthBounds{ 0.0f },
		.maxDepthBounds{ 1.0f },
	};
}

std::span<uint8_t> GraphicsPipelineWrapper::mapPushConstant()
{
	return { pushConstant.buffer };
}

std::span<uint8_t const> GraphicsPipelineWrapper::readPushConstant() const
{
	return { pushConstant.buffer };
}
