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
        , std::span<GPUTypes::LightDirectional const> directionalLights
        , std::span<GPUTypes::LightSpot const> spotLights
        , uint32_t viewCameraIndex
        , TStagedBuffer<GPUTypes::Camera> const& cameras
        , uint32_t atmosphereIndex
        , TStagedBuffer<GPUTypes::Atmosphere> const& atmospheres
        , SceneBounds const& sceneBounds
        , MeshAsset const& sceneMesh
        , MeshInstances const& sceneGeometry
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
    ShadowPassArray m_shadowPassArray{};

    VmaAllocator m_allocator{ VK_NULL_HANDLE };

    std::unique_ptr<TStagedBuffer<GPUTypes::LightDirectional>> m_directionalLights{};
    std::unique_ptr<TStagedBuffer<GPUTypes::LightSpot>> m_spotLights{};

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

        VkDeviceAddress directionalLightsBuffer{};
        VkDeviceAddress spotLightsBuffer{};

        uint32_t directionalLightCount{};
        uint32_t spotLightCount{};
        uint32_t atmosphereIndex{ 0 };
        uint32_t cameraIndex{ 0 };
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

public:
    struct Parameters
    {
        ShadowPassParameters shadowPassParameters{};
    };
    Parameters m_parameters;
};