#pragma once

#include "engine_types.h"
#include "shaders.hpp"

class PipelineBuilder {
public:
    PipelineBuilder() {};

    VkPipeline buildPipeline(VkDevice device, VkPipelineLayout layout) const;
    void setShaders(ShaderWrapper const& vertexShader, ShaderWrapper const& fragmentShader);
    void setInputTopology(VkPrimitiveTopology topology);
    void setPolygonMode(VkPolygonMode mode);
    void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void setMultisamplingNone();
    void disableBlending();
    void setColorAttachmentFormat(VkFormat format);
    void setDepthFormat(VkFormat format);
    void disableDepthTest();

private:
	std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages{};

	VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{
        .sType{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO },
    };
	VkPipelineRasterizationStateCreateInfo m_rasterizer{
        .sType{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO }
    };
    VkPipelineColorBlendAttachmentState m_colorBlendAttachment{};
    VkPipelineMultisampleStateCreateInfo m_multisampling{
        .sType{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO }
    };
    VkPipelineDepthStencilStateCreateInfo m_depthStencil{
        .sType{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO }
    };

    VkFormat m_colorAttachmentFormat{ VK_FORMAT_UNDEFINED };
    VkFormat m_depthAttachmentFormat{ VK_FORMAT_UNDEFINED };
};

struct GraphicsPipelineWrapper
{
    ShaderWrapper vertexShader{};
    ShaderWrapper fragmentShader{};
    VkPipeline pipeline{ VK_NULL_HANDLE };
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

    void cleanup(VkDevice device) {
        vertexShader.cleanup(device);
        fragmentShader.cleanup(device);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
    }
};

namespace vkutil
{
	ShaderWrapper loadShaderModule(std::string const& localPath, VkDevice device);
}