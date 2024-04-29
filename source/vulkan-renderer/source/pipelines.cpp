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

template<class... Ts>
struct overloaded : Ts...
{
	using Ts::operator()...;
};
static std::optional<ShaderModuleReflected> loadShaderModule(
	VkDevice device
	, std::string path
)
{
	AssetLoadingResult const fileLoadingResult{ loadAssetFile(path, device) };

	return std::visit(
		overloaded{
			[&](AssetFile const& file)
			{
				return std::optional<ShaderModuleReflected>{ShaderModuleReflected::FromBytecode(
					device
					, file.fileName
					, file.fileBytes
				)};
			},
			[&](AssetLoadingError const& error)
			{
				Error(fmt::format("Failed to load asset for shader: {}", error.message));
				return std::optional<ShaderModuleReflected>{};
			}
		}, fileLoadingResult
	);
}
static std::optional<ShaderObjectReflected> loadShaderObject(
	VkDevice device
	, std::string path
	, VkShaderStageFlagBits stage
	, VkShaderStageFlags nextStage
	, std::span<VkDescriptorSetLayout const> layouts
	, VkSpecializationInfo specializationInfo
)
{
	AssetLoadingResult const fileLoadingResult{ loadAssetFile(path, device) };

	return std::visit(
		overloaded{
			[&](AssetFile const& file)
			{
				return std::optional<ShaderObjectReflected>{ShaderObjectReflected::FromBytecodeReflected(
					device
					, file.fileName
					, file.fileBytes
					, stage
					, nextStage
					, layouts
					, specializationInfo
				)};
			},
			[&](AssetLoadingError const& error)
			{
				Error(fmt::format("Failed to load asset for shader: {}", error.message));
				return std::optional<ShaderObjectReflected>{};
			}
		}, fileLoadingResult
	);
}

InstancedMeshGraphicsPipeline::InstancedMeshGraphicsPipeline(
	VkDevice device
	, VkFormat colorAttachmentFormat
	, VkFormat depthAttachmentFormat
	, VkDescriptorSetLayout drawTargetDescriptor
)
{
	ShaderModuleReflected const vertexShader{ loadShaderModule(device, "shaders/blinnphong/phong.vert.spv").value() };
	ShaderModuleReflected const fragmentShader{ loadShaderModule(device, "shaders/blinnphong/phong.frag.spv").value() };

	std::vector<VkPushConstantRange> pushConstantRanges{};
	{ 
		// Vertex push constant
		ShaderReflectionData::PushConstant const& vertexPushConstant{ vertexShader.reflectionData().defaultPushConstant()};

		size_t const vertexPushConstantSize{ vertexPushConstant.type.paddedSizeBytes - vertexPushConstant.layoutOffsetBytes };
		size_t const vertexPushConstantSizeExpected{ sizeof(VertexPushConstant) };

		if (vertexPushConstantSize != vertexPushConstantSizeExpected) {
			Warning(fmt::format("Loaded vertex push constant had a push constant of size {}, while implementation expects {}."
				, vertexPushConstantSize
				, vertexPushConstantSizeExpected
			));
		}

		pushConstantRanges.push_back(vertexPushConstant.totalRange(VK_SHADER_STAGE_VERTEX_BIT));
		
		// Fragment push constant
		ShaderReflectionData::PushConstant const& fragmentPushConstant{ fragmentShader.reflectionData().defaultPushConstant() };

		size_t const fragmentPushConstantSize{ fragmentPushConstant.type.paddedSizeBytes - fragmentPushConstant.layoutOffsetBytes };
		size_t const fragmentPushConstantSizeExpected{ sizeof(FragmentPushConstant) };

		if (fragmentPushConstantSize != fragmentPushConstantSizeExpected) {
			Warning(fmt::format("Loaded fragment push constant had a push constant of size {}, while implementation expects {}."
				, fragmentPushConstantSize
				, fragmentPushConstantSizeExpected
			));
		}

		pushConstantRanges.push_back(fragmentPushConstant.totalRange(VK_SHADER_STAGE_FRAGMENT_BIT));

		// We don't pack the push constants super tight for now
		if (fragmentPushConstant.layoutOffsetBytes < vertexPushConstant.type.paddedSizeBytes)
		{
			Warning("Fragment push constant overlaps with vertex push constant.");
		}
	}

	VkPipelineLayoutCreateInfo const layoutInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO },
		.pNext{ nullptr },

		.flags{ 0 },

		.setLayoutCount{ 1 },
		.pSetLayouts{ &drawTargetDescriptor },

		.pushConstantRangeCount{ static_cast<uint32_t>(pushConstantRanges.size()) },
		.pPushConstantRanges{ pushConstantRanges.data() },
	};

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	CheckVkResult(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

	PipelineBuilder pipelineBuilder{};
	pipelineBuilder.pushShader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
	pipelineBuilder.pushShader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.setMultisamplingNone();
	pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	pipelineBuilder.setColorAttachment(colorAttachmentFormat);
	pipelineBuilder.setDepthFormat(depthAttachmentFormat);


	m_vertexShader = vertexShader;
	m_fragmentShader = fragmentShader;

	m_graphicsPipelineLayout = pipelineLayout;
	m_graphicsPipeline = pipelineBuilder.buildPipeline(device, pipelineLayout);
}

void InstancedMeshGraphicsPipeline::recordDrawCommands(
	VkCommandBuffer cmd
	, bool reuseDepthAttachment
	, AllocatedImage const& color
	, AllocatedImage const& depth
	, VkDescriptorSet shadowMapSet
	, uint32_t cameraIndex
	, uint32_t cameraIndexShadowPass
	, TStagedBuffer<GPUTypes::Camera> const& cameras
	, uint32_t atmosphereIndex
	, TStagedBuffer<GPUTypes::Atmosphere> const& atmospheres
	, MeshAsset const& mesh
	, TStagedBuffer<glm::mat4x4> const& models
	, TStagedBuffer<glm::mat4x4> const& modelInverseTransposes
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

	VkExtent2D const drawExtent{
		.width{color.imageExtent.width},
		.height{color.imageExtent.height},
	};
	std::vector<VkRenderingAttachmentInfo> const colorAttachments{ colorAttachment };
	VkRenderingInfo const renderInfo{
		vkinit::renderingInfo(drawExtent, colorAttachments, &depthAttachment)
	};

	cameras.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
	models.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
	modelInverseTransposes.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &shadowMapSet, 0, nullptr);

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
			.modelInverseTransposeBufferAddress{ modelInverseTransposes.deviceAddress() },
			.cameraBufferAddress{ cameras.deviceAddress() },
			.cameraIndex{ cameraIndex },
			.shadowpassCameraIndex{ cameraIndexShadowPass },
		};
		vkCmdPushConstants(cmd, m_graphicsPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(VertexPushConstant), &vertexPushConstant
		);
		m_vertexPushConstant = vertexPushConstant;
	}

	{ // Fragment push constant
		GPUTypes::Atmosphere const& currentAtmosphere{ atmospheres.readValidStaged()[atmosphereIndex] };

		FragmentPushConstant const fragmentPushConstant{
			.lightDirectionViewSpace{ cameras.readValidStaged()[cameraIndex].view * glm::vec4(currentAtmosphere.directionToSun,0.0) },
			.diffuseColor{ glm::vec4(0.8) },
			.specularColor{ glm::vec4(1.0) },
			.atmosphereBuffer{ atmospheres.deviceAddress() },
			.atmosphereIndex{ atmosphereIndex },
			.shininess{ 32.0 }
		};
		vkCmdPushConstants(cmd, m_graphicsPipelineLayout,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			m_fragmentShader.reflectionData().defaultPushConstant().layoutOffsetBytes, sizeof(FragmentPushConstant), &fragmentPushConstant
		);
		m_fragmentPushConstant = fragmentPushConstant;
	}

	GeometrySurface const& drawnSurface{ mesh.surfaces[0] };

	// Bind the entire index buffer of the mesh, but only draw a single surface.
	vkCmdBindIndexBuffer(cmd, meshBuffers.indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, drawnSurface.indexCount, models.deviceSize(), drawnSurface.firstIndex, 0, 0);

	vkCmdEndRendering(cmd);
}

void InstancedMeshGraphicsPipeline::cleanup(VkDevice device)
{
	m_vertexShader.cleanup(device);
	m_fragmentShader.cleanup(device);

	vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, m_graphicsPipelineLayout, nullptr);
}

AtmosphereComputePipeline::AtmosphereComputePipeline(
	VkDevice device, 
	VkDescriptorSetLayout drawImageDescriptorLayout
)
{
	ShaderModuleReflected const skyShader{ loadShaderModule(device, "shaders/sky.comp.spv").value() };

	ShaderReflectionData::PushConstant const& skyPushConstant{ skyShader.reflectionData().defaultPushConstant() };
	
	{
		size_t const skyShaderPushConstantSize{ skyPushConstant.type.paddedSizeBytes };
		size_t const pushConstantSize{ sizeof(PushConstantType) };

		if (skyShaderPushConstantSize != pushConstantSize)
		{
			Warning(fmt::format("Loaded shader had a push constant of size {}, while implementation expects {}."
				, skyShaderPushConstantSize
				, pushConstantSize
			));
		}
	}

	VkPushConstantRange const pushConstantRange{
		.stageFlags{ VK_SHADER_STAGE_COMPUTE_BIT },
		.offset{ 0 },
		.size{ sizeof(PushConstantType) },
	};

	VkPipelineLayoutCreateInfo const layoutInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO },
		.pNext{ nullptr },

		.flags{ 0 },

		.setLayoutCount{ 1 },
		.pSetLayouts{ &drawImageDescriptorLayout },

		.pushConstantRangeCount{ 1 },
		.pPushConstantRanges{ &pushConstantRange },
	};

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	CheckVkResult(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

	VkPipelineShaderStageCreateInfo const stageInfo{
		.sType{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		.pNext{ nullptr },

		.stage{ VK_SHADER_STAGE_COMPUTE_BIT },
		.module{ skyShader.shaderModule() },
		.pName{ skyShader.reflectionData().defaultEntryPoint.c_str()},
	};

	VkComputePipelineCreateInfo const computePipelineCreateInfo{
		.sType{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO },
		.pNext{ nullptr },

		.stage{ stageInfo },
		.layout{ pipelineLayout },
	};

	VkPipeline pipeline{ VK_NULL_HANDLE };
	CheckVkResult(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));

	m_skyShader = skyShader;

	m_computePipelineLayout = pipelineLayout;
	m_computePipeline = pipeline;
}

void AtmosphereComputePipeline::recordDrawCommands(
	VkCommandBuffer cmd
	, uint32_t cameraIndex
	, TStagedBuffer<GPUTypes::Camera> const& camerasBuffer
	, uint32_t atmosphereIndex
	, TStagedBuffer<GPUTypes::Atmosphere> const& atmospheresBuffer
	, VkDescriptorSet colorSet
	, VkExtent2D colorExtent
) const
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &colorSet, 0, nullptr);

	camerasBuffer.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
	atmospheresBuffer.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

	m_pushConstant = PushConstantType{
		.cameraIndex{ cameraIndex },
		.atmosphereIndex{ atmosphereIndex },
		.cameraBuffer{ camerasBuffer.deviceAddress() },
		.atmosphereBuffer{ atmospheresBuffer.deviceAddress() },
	};

	vkCmdPushConstants(cmd, m_computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0
		, sizeof(PushConstantType), &m_pushConstant
	);

	vkCmdDispatch(cmd, std::ceil(colorExtent.width / 16.0), std::ceil(colorExtent.height / 16.0), 1);
}

void AtmosphereComputePipeline::cleanup(VkDevice device)
{
	m_skyShader.cleanup(device);

	vkDestroyPipeline(device, m_computePipeline, nullptr);
	vkDestroyPipelineLayout(device, m_computePipelineLayout, nullptr);
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
			loadShaderObject(
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
		std::span<uint8_t const> pushConstantBytes{ readPushConstantBytes() };
		uint32_t const offset{ reflectionData.defaultPushConstant().layoutOffsetBytes };

		vkCmdPushConstants(cmd, layout, stage, offset, pushConstantBytes.size() - offset, pushConstantBytes.data() + offset);
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
	ShaderModuleReflected const vertexShader{ loadShaderModule(device, "shaders/debug/debugline.vert.spv").value() };
	ShaderModuleReflected const fragmentShader{ loadShaderModule(device, "shaders/debug/debugline.frag.spv").value() };

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

	VkExtent2D const drawExtent{
		.width{color.imageExtent.width},
		.height{color.imageExtent.height},
	};
	std::vector<VkRenderingAttachmentInfo> const colorAttachments{ colorAttachment };
	VkRenderingInfo const renderInfo{
		vkinit::renderingInfo(drawExtent, colorAttachments, &depthAttachment)
	};

	cameras.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
	endpoints.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
	indices.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT);

	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	vkCmdSetLineWidth(cmd, lineWidth);

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
	ShaderModuleReflected const vertexShader{ loadShaderModule(device, "shaders/offscreenpass/depthpass.vert.spv").value() };

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

	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
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
	, uint32_t cameraIndex
	, TStagedBuffer<GPUTypes::Camera> const& cameras
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
		vkinit::renderingInfo(drawExtent, {}, &depthAttachment)
	};

	cameras.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
	models.recordTotalCopyBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

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

	vkCmdSetDepthBias(cmd, depthBias, 0.0, depthBiasSlope);

	GPUMeshBuffers& meshBuffers{ *mesh.meshBuffers };

	{ // Vertex push constant
		VertexPushConstant const vertexPushConstant{
			.vertexBufferAddress{ meshBuffers.vertexAddress() },
			.modelBufferAddress{ models.deviceAddress() },
			.cameraBufferAddress{ cameras.deviceAddress() },
			.cameraIndex{ cameraIndex },
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
