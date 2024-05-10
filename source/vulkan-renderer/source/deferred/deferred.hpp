#pragma once

#include "../enginetypes.hpp"
#include "../engineparams.hpp"
#include "../pipelines.hpp"
#include "../shadowpass.hpp"

#include "gbuffer.hpp"

class DeferredShadingPipeline
{
public:
    DeferredShadingPipeline(
        VkDevice device
        , VmaAllocator allocator
        , DescriptorAllocator& descriptorAllocator
        , VkExtent3D drawExtent
    );

    void recordDrawCommands(
        VkCommandBuffer cmd
        , AllocatedImage const& color
        , AllocatedImage const& depth
        , float depthBiasConstant
        , float depthBiasSlope
        , uint32_t sunCameraIndex
        , uint32_t viewCameraIndex
        , TStagedBuffer<GPUTypes::Camera> const& cameras
        , uint32_t atmosphereIndex
        , TStagedBuffer<GPUTypes::Atmosphere> const& atmospheres
        , MeshAsset const& mesh
        , TStagedBuffer<glm::mat4x4> const& models
        , TStagedBuffer<glm::mat4x4> const& modelInverseTransposes
    );

    void updateRenderTargetDescriptors(
        VkDevice device
        , AllocatedImage const& drawImage
        , AllocatedImage const& depthImage
    );
    
    void cleanup(
        VkDevice device
        , VmaAllocator allocator
    );

private:
    // This pipeline supports one light: the sun as a directional light 
    ShadowPass m_shadowPass{};
    ShadowPassParameters m_shadowPassParameters{}; // TODO: derive shadow pass parameters from this and drive via UI

    VkDescriptorSet m_drawImageSet{ VK_NULL_HANDLE };
    VkDescriptorSetLayout m_drawImageLayout{ VK_NULL_HANDLE }; // Used by compute shaders to output final image

    VkDescriptorSet m_depthImageSet{ VK_NULL_HANDLE };
    VkDescriptorSetLayout m_depthImageLayout{ VK_NULL_HANDLE }; // Used by compute shaders to read scene depth

    VkSampler m_depthImageImmutableSampler{ VK_NULL_HANDLE };

    GBuffer m_gBuffer{};

    struct GBufferVertexPushConstant
    {
        VkDeviceAddress vertexBuffer{};
        VkDeviceAddress modelBuffer{};

        VkDeviceAddress modelInverseTransposeBuffer{};
        VkDeviceAddress cameraBuffer{};

        uint32_t cameraIndex{ 0 };
        uint8_t padding0[12]{};
    };

    GBufferVertexPushConstant mutable m_gBufferVertexPushConstant{};
    ShaderObjectReflected m_gBufferVertexShader{ ShaderObjectReflected::MakeInvalid() };
    ShaderObjectReflected m_gBufferFragmentShader{ ShaderObjectReflected::MakeInvalid() };
    VkPipelineLayout m_gBufferLayout{ VK_NULL_HANDLE }; //TODO: initialize these layouts

    struct LightingPassComputePushConstant
    {
        VkDeviceAddress cameraBuffer{};
        VkDeviceAddress atmosphereBuffer{};

        uint32_t atmosphereIndex{ 0 };
        uint32_t cameraIndex{ 0 };
        uint32_t cameraDirectionalLightIndex{ 0 };
        uint8_t padding0[4]{};
    };

    LightingPassComputePushConstant mutable m_lightingPassPushConstant{};
    ShaderObjectReflected m_lightingPassComputeShader{ ShaderObjectReflected::MakeInvalid() };
    VkPipelineLayout m_lightingPassLayout{ VK_NULL_HANDLE };

    struct SkyPassComputePushConstant
    {
        VkDeviceAddress atmosphereBuffer{};
        VkDeviceAddress cameraBuffer{};

        uint32_t atmosphereIndex{ 0 };
        uint32_t cameraIndex{ 0 };
        uint8_t padding0[8]{};
    };

    SkyPassComputePushConstant mutable m_skyPassPushConstant{};
    ShaderObjectReflected m_skyPassComputeShader{ ShaderObjectReflected::MakeInvalid() };
    VkPipelineLayout m_skyPassLayout{ VK_NULL_HANDLE };
};