#pragma once

#include <type_traits>

#include "engine_types.h"

#include "gputypes.hpp"

#include "shaders.hpp"
#include "assets.hpp"
#include "buffers.hpp"

class PipelineBuilder {
public:
    PipelineBuilder() {};

    VkPipeline buildPipeline(VkDevice device, VkPipelineLayout layout) const;
    void setShaders(ShaderModuleReflected const& vertexShader, ShaderModuleReflected const& fragmentShader);
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

/*
* A graphics pipeline that renders multiple instances of a single mesh.
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
    ShaderModuleReflected m_vertexShader{ ShaderModuleReflected::MakeInvalid() };
    ShaderModuleReflected m_fragmentShader{ ShaderModuleReflected::MakeInvalid() };
    
    VkPipeline m_graphicsPipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_graphicsPipelineLayout{ VK_NULL_HANDLE };

    struct PushConstantType {
        glm::mat4x4 cameraTransform{};

        VkDeviceAddress vertexBufferAddress{};
        VkDeviceAddress transformBufferAddress{};
    };

    PushConstantType m_pushConstant;

public:
    PushConstantType const& pushConstant() const { return m_pushConstant; };
};

/*
* A compute pipeline that renders the scattering of sunlight in the sky.
* It models all light from the sun as reaching the camera via primary scattering,
* so it produces inaccurate results in thin atmospheres.
*/
class BackgroundComputePipeline
{
public:
    BackgroundComputePipeline(
        VkDevice device, 
        VkDescriptorSetLayout drawImageDescriptorLayout
    );

    void recordDrawCommands(
        VkCommandBuffer cmd,
        uint32_t cameraIndex,
        TStagedBuffer<GPUTypes::Camera> const& camerasBuffer,
        uint32_t atmosphereIndex,
        TStagedBuffer<GPUTypes::Atmosphere> const& atmospheresBuffer,
        VkDescriptorSet colorSet,
        VkExtent2D colorExtent
    ) const;

    void cleanup(VkDevice device);

    std::span<uint8_t const> pushConstantBytes() const
    {
        return std::span<uint8_t const>(
            reinterpret_cast<uint8_t const*>(&m_pushConstant)
            , sizeof(PushConstantType)
        );
    }

private:
    ShaderModuleReflected m_skyShader{ ShaderModuleReflected::MakeInvalid() };

    VkPipeline m_computePipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_computePipelineLayout{ VK_NULL_HANDLE };

    struct PushConstantType {
        uint32_t cameraIndex{};
        uint8_t padding0[4];

        uint32_t atmosphereIndex{};
        uint8_t padding1[4];

        VkDeviceAddress cameraBuffer{};
        VkDeviceAddress atmosphereBuffer{};
    };

    /*
    * A cached copy of the push constants that were last sent to the GPU during recording.
    * This value is not read by the pipeline and is only useful to reflect the intended state on the GPU.
    */
    PushConstantType mutable m_pushConstant;

public:
    PushConstantType const& pushConstant() const { return m_pushConstant; };
    ShaderReflectionData::PushConstant const& pushConstantReflected() const 
    { 
        return m_skyShader.reflectionData().defaultPushConstant(); 
    };
};

/*
* A generic compute pipeline driven entirely by a push constant.
*/
class GenericComputePipeline
{
public:
    GenericComputePipeline(
        VkDevice device,
        VkDescriptorSetLayout drawImageDescriptorLayout
    );

    void recordDrawCommands(
        VkCommandBuffer cmd
    ) const;

    void cleanup(VkDevice device);

    std::span<uint8_t const> pushConstantBytes() const;

private:
    std::vector<ShaderObjectReflected> m_shaders{};
};