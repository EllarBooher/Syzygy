#pragma once

#include <type_traits>

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

struct PipelinePushConstant
{
    /** 
        Indicates that a push constant's value is driven by the engine.
        Useful to indicate to UI to make it readonly, or not read at all.
    */
    bool engineDriven{ true };
    VkShaderStageFlags pipelineStages{};
    std::vector<uint8_t> buffer{};
    ShaderReflectionData::PushConstant reflectionData{};
};

struct GraphicsPipelineWrapper
{
    ShaderWrapper vertexShader{};
    ShaderWrapper fragmentShader{};
    VkPipeline pipeline{ VK_NULL_HANDLE };
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
    PipelinePushConstant pushConstant{};

    template<typename T>
    inline T readPushConstant(size_t byteOffset = 0) const {
        static_assert(
            std::is_trivially_copyable<T>::value,
            "push constant T must be trivially copyable"
        );
        assert(sizeof(T) + byteOffset <= pushConstant.buffer.size());
        T const* const pValue{ reinterpret_cast<T const*>(pushConstant.buffer.data() + byteOffset) };
        return *pValue;
    }

    template<typename T>
    inline void setPushConstant(T value, size_t byteOffset = 0) {
        static_assert(
            std::is_trivially_copyable<T>::value, 
            "push constant T must be trivially copyable"
        );
        assert(sizeof(T) + byteOffset <= pushConstant.buffer.size());
        memcpy(pushConstant.buffer.data() + byteOffset, &value, sizeof(T));
    }

    std::span<uint8_t> mapPushConstant();
    std::span<uint8_t const> readPushConstant() const;

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