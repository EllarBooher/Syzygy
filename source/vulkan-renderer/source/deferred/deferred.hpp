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
        , VkExtent2D dimensionCapacity
    );

    void recordDrawCommands(
        VkCommandBuffer cmd
        , VkRect2D drawRect
        , VkImageLayout colorLayout
        , AllocatedImage const& color
        , AllocatedImage const& depth
        , std::span<GPUTypes::LightDirectional const> directionalLights
        , std::span<GPUTypes::LightSpot const> spotLights
        , uint32_t viewCameraIndex
        , TStagedBuffer<GPUTypes::Camera> const& cameras
        , uint32_t atmosphereIndex
        , TStagedBuffer<GPUTypes::Atmosphere> const& atmospheres
        , SceneBounds const& sceneBounds
        , bool renderMesh
        , MeshAsset const& sceneMesh
        , MeshInstances const& sceneGeometry
    );

    void updateRenderTargetDescriptors(
        VkDevice device
        , AllocatedImage const& depthImage
    );
    
    void cleanup(
        VkDevice device
        , VmaAllocator allocator
    );

private:
    ShadowPassArray m_shadowPassArray{};

    AllocatedImage m_drawImage{};

    VmaAllocator m_allocator{ VK_NULL_HANDLE };

    typedef TStagedBuffer<GPUTypes::LightDirectional> LightDirectionalBuffer;
    std::unique_ptr<LightDirectionalBuffer> m_directionalLights{};

    typedef TStagedBuffer<GPUTypes::LightSpot> LightSpotBuffer;
    std::unique_ptr<LightSpotBuffer> m_spotLights{};

    VkDescriptorSet m_drawImageSet{ VK_NULL_HANDLE };
    // Used by compute shaders to output final image
    VkDescriptorSetLayout m_drawImageLayout{ VK_NULL_HANDLE }; 

    VkDescriptorSet m_depthImageSet{ VK_NULL_HANDLE };
    // Used by compute shaders to read scene depth
    VkDescriptorSetLayout m_depthImageLayout{ VK_NULL_HANDLE }; 

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

    GBufferVertexPushConstant /* mutable */ m_gBufferVertexPushConstant{};
    ShaderObjectReflected m_gBufferVertexShader{ 
        ShaderObjectReflected::makeInvalid() 
    };
    ShaderObjectReflected m_gBufferFragmentShader{ 
        ShaderObjectReflected::makeInvalid() 
    };

    // TODO: initialize these layouts
    VkPipelineLayout m_gBufferLayout{ VK_NULL_HANDLE };

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

        glm::vec2 gbufferOffset{};
        glm::vec2 gbufferExtent{};
    };

    LightingPassComputePushConstant /* mutable */ m_lightingPassPushConstant{};
    
    ShaderObjectReflected m_lightingPassComputeShader{ 
        ShaderObjectReflected::makeInvalid() 
    };
    
    VkPipelineLayout m_lightingPassLayout{ VK_NULL_HANDLE };

    struct SkyPassComputePushConstant
    {
        VkDeviceAddress atmosphereBuffer{};
        VkDeviceAddress cameraBuffer{};

        uint32_t atmosphereIndex{ 0 };
        uint32_t cameraIndex{ 0 };
        
        glm::vec2 drawOffset{};
        glm::vec2 drawExtent{};

        uint8_t padding0[8]{};
    };

    SkyPassComputePushConstant /* mutable */ m_skyPassPushConstant{};
    
    ShaderObjectReflected m_skyPassComputeShader{ 
        ShaderObjectReflected::makeInvalid() 
    };
    
    VkPipelineLayout m_skyPassLayout{ VK_NULL_HANDLE };

public:
    struct Parameters
    {
        ShadowPassParameters shadowPassParameters{};
    };
    Parameters m_parameters;
};