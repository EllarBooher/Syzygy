#pragma once

#include <type_traits>

#include "engine_types.h"
#include "shaders.hpp"
#include "assets.hpp"
#include "buffers.hpp"

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
    void enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp);

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

/**
    A graphics pipeline that manages its own resources and can render an array of matrices.
*/
class InstancedMeshGraphicsPipeline
{
public:
    InstancedMeshGraphicsPipeline(
        VkDevice device,
        VkFormat colorAttachmentFormat,
        VkFormat depthAttachmentFormat
    );

    void recordDrawCommands(
        VkCommandBuffer cmd, 
        glm::mat4x4 camera,
        bool reuseDepthAttachment,
        AllocatedImage const& color,
        AllocatedImage const& depth,
        MeshAsset const& mesh,
        TStagedBuffer<glm::mat4x4> const& transforms
    ) const;

    void cleanup(VkDevice device);

private:
    ShaderWrapper m_vertexShader{};
    ShaderWrapper m_fragmentShader{};
    
    VkPipeline m_graphicsPipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_graphicsPipelineLayout{ VK_NULL_HANDLE };

    struct PushConstantType {
        glm::mat4x4 cameraTransform{};

        VkDeviceAddress vertexBufferAddress{};
        VkDeviceAddress transformBufferAddress{};
    };
};

class BackgroundComputePipeline
{
public:
    BackgroundComputePipeline(
        VkDevice device, 
        VkDescriptorSetLayout drawImageDescriptorLayout
    );

    void recordDrawCommands(
        VkCommandBuffer cmd,
        double aspectRatio,
        CameraParameters const& camera,
        VkDescriptorSet colorSet,
        VkExtent2D colorExtent
    ) const;

    void cleanup(VkDevice device);

private:
    ShaderWrapper m_skyShader{};

    VkPipeline m_computePipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_computePipelineLayout{ VK_NULL_HANDLE };

    struct PushConstantType {
        glm::mat4x4 inverseProjection{};
        glm::mat4x4 rotation{};

        glm::vec3 cameraPosition{};
        uint8_t padding0[4];
    };
};

namespace vkutil
{
	ShaderWrapper loadShaderModule(std::string const& localPath, VkDevice device);
}