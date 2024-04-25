#pragma once

#include <type_traits>
#include <set>

#include "enginetypes.hpp"

#include "gputypes.hpp"

#include "shaders.hpp"
#include "assets.hpp"
#include "buffers.hpp"

struct DrawResultsGraphics
{
    size_t drawCalls{ 0 };
    size_t verticesDrawn{ 0 };
    size_t indicesDrawn{ 0 };
};

class PipelineBuilder {
public:
    PipelineBuilder() {};

    VkPipeline buildPipeline(VkDevice device, VkPipelineLayout layout) const;
    void setShaders(ShaderModuleReflected const& vertexShader, ShaderModuleReflected const& fragmentShader);
    void setInputTopology(VkPrimitiveTopology topology);
    void setPolygonMode(VkPolygonMode mode);
    void pushDynamicState(VkDynamicState dynamicState);
    void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void setMultisamplingNone();
    void disableBlending();
    void setColorAttachmentFormat(VkFormat format);
    void setDepthFormat(VkFormat format);

    void disableDepthTest();
    void enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp);

private:
	std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages{};
    std::set<VkDynamicState> m_dynamicStates{};

	VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{
        .sType{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO },
    };
	VkPipelineRasterizationStateCreateInfo m_rasterizer{
        .sType{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO },
        .lineWidth{ 1.0 },
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
        VkCommandBuffer cmd
        , bool reuseDepthAttachment
        , AllocatedImage const& color
        , AllocatedImage const& depth
        , uint32_t cameraIndex
        , TStagedBuffer<GPUTypes::Camera> const& cameras
        , uint32_t atmosphereIndex
        , TStagedBuffer<GPUTypes::Atmosphere> const& atmospheres
        , MeshAsset const& mesh
        , TStagedBuffer<glm::mat4x4> const& models
        , TStagedBuffer<glm::mat4x4> const& modelInverseTransposes
    ) const;

    void cleanup(VkDevice device);

private:
    ShaderModuleReflected m_vertexShader{ ShaderModuleReflected::MakeInvalid() };
    ShaderModuleReflected m_fragmentShader{ ShaderModuleReflected::MakeInvalid() };
    
    VkPipeline m_graphicsPipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_graphicsPipelineLayout{ VK_NULL_HANDLE };

    struct VertexPushConstant {
        VkDeviceAddress vertexBufferAddress{};
        VkDeviceAddress modelBufferAddress{};

        VkDeviceAddress modelInverseTransposeBufferAddress{};
        VkDeviceAddress cameraBufferAddress{};

        uint32_t cameraIndex{ 0 };
        uint8_t padding0[12]{};
    };

    struct FragmentPushConstant {
        glm::vec4 lightDirectionViewSpace{};

        glm::vec4 diffuseColor{};

        glm::vec4 specularColor{};

        VkDeviceAddress atmosphereBuffer{};
        uint32_t atmosphereIndex{ 0 };
        float shininess{ 0.0f };
    };

    VertexPushConstant mutable m_vertexPushConstant{};
    FragmentPushConstant mutable m_fragmentPushConstant{};

public:
    ShaderModuleReflected const& vertexShader() const { return m_vertexShader; };
    ShaderModuleReflected const& fragmentShader() const { return m_fragmentShader; };
    VertexPushConstant const& vertexPushConstant() const { return m_vertexPushConstant; };
    ShaderReflectionData::PushConstant const& vertexPushConstantReflected() const { return m_vertexShader.reflectionData().defaultPushConstant(); };

    FragmentPushConstant const& fragmentPushConstant() const { return m_fragmentPushConstant; };
    ShaderReflectionData::PushConstant const& fragmentPushConstantReflected() const { return m_fragmentShader.reflectionData().defaultPushConstant(); };
};

/*
* A compute pipeline that renders the scattering of sunlight in the sky.
* It models all light from the sun as reaching the camera via primary scattering,
* so it produces inaccurate results in thin atmospheres.
*/
class AtmosphereComputePipeline
{
public:
    AtmosphereComputePipeline(
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

    ShaderModuleReflected const& shader() const { return m_skyShader; };

private:
    ShaderModuleReflected m_skyShader{ ShaderModuleReflected::MakeInvalid() };

    VkPipeline m_computePipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_computePipelineLayout{ VK_NULL_HANDLE };

    struct PushConstantType {
        uint32_t cameraIndex{};
        uint32_t atmosphereIndex{};
        VkDeviceAddress cameraBuffer{};

        VkDeviceAddress atmosphereBuffer{};
        uint8_t padding0[8]{};
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
* Supports multiple shader objects, swapping between them and only using one of them during dispatch.
*/
class GenericComputeCollectionPipeline
{
public:
    GenericComputeCollectionPipeline(
        VkDevice device
        , VkDescriptorSetLayout drawImageDescriptorLayout
        , std::span<std::string const> shaderPaths
    );

    void recordDrawCommands(
        VkCommandBuffer cmd
        , VkDescriptorSet drawImageDescriptors
        , VkExtent2D drawExtent
    ) const;

    void cleanup(VkDevice device);

    std::span<uint8_t> mapPushConstantBytes()
    {
        return m_shaderPushConstants[m_shaderIndex];
    }
    std::span<uint8_t const> readPushConstantBytes() const
    {
        return m_shaderPushConstants[m_shaderIndex];
    }

    ShaderObjectReflected const& currentShader() const
    {
        return m_shaders[m_shaderIndex];
    }
    VkPipelineLayout currentLayout() const
    {
        return m_layouts[m_shaderIndex];
    }

    void selectShader(size_t index)
    {
        size_t const count{ m_shaders.size() };
        if (count == 0)
        {
            return;
        }
        else if (index >= count)
        {
            Warning(fmt::format("Shader index {} is out of bounds of {}", index, count));
            return;
        }

        m_shaderIndex = index;
    }
    size_t shaderIndex() const { return m_shaderIndex; };
    size_t shaderCount() const { return m_shaders.size(); };
    std::span<ShaderObjectReflected const> shaders() const { return m_shaders; };

private:
    size_t m_shaderIndex{ 0 };

    std::vector<ShaderObjectReflected> m_shaders{};
    std::vector<std::vector<uint8_t>> m_shaderPushConstants{};
    std::vector<VkPipelineLayout> m_layouts{}; 
};

/*
* A pipeline that draws debug geometry such as lines and points in a compute pass
* TODO: combine this with the other compute pipeline
*/
class DebugLineComputePipeline
{
public:
    DebugLineComputePipeline(
        VkDevice device,
        VkFormat colorAttachmentFormat,
        VkFormat depthAttachmentFormat
    );

    DrawResultsGraphics recordDrawCommands(
        VkCommandBuffer cmd
        , bool reuseDepthAttachment
        , float lineWidth
        , AllocatedImage const& color
        , AllocatedImage const& depth
        , uint32_t cameraIndex
        , TStagedBuffer<GPUTypes::Camera> const& cameras
        , TStagedBuffer<Vertex> const& endpoints
        , TStagedBuffer<uint32_t> const& indices
    ) const;

    void cleanup(VkDevice device);

private:
    ShaderModuleReflected m_vertexShader{ ShaderModuleReflected::MakeInvalid() };
    ShaderModuleReflected m_fragmentShader{ ShaderModuleReflected::MakeInvalid() };

    struct VertexPushConstant
    {
        VkDeviceAddress vertexBuffer{};
        VkDeviceAddress cameraBuffer{};

        uint32_t cameraIndex{ 0 };
        uint8_t padding0[12]{};
    };

    VertexPushConstant mutable m_vertexPushConstant{};

    VkPipeline m_graphicsPipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_graphicsPipelineLayout{ VK_NULL_HANDLE };

public:
    ShaderModuleReflected const& vertexShader() const { return m_vertexShader; };
    VertexPushConstant const& vertexPushConstant() const { return m_vertexPushConstant; };

    ShaderModuleReflected const& fragmentShader() const { return m_fragmentShader; };
};