#pragma once

#include "enginetypes.hpp"
#include "descriptors.hpp"
#include "pipelines.hpp"
#include "images.hpp"


// Handles the resources for an array of depth maps, 
// which share a sampler and should be accessed via a descriptor array.
class ShadowPassArray
{
public:
    static std::optional<ShadowPassArray> create(
        VkDevice device
        , DescriptorAllocator& descriptorAllocator
        , VmaAllocator allocator
        , uint32_t shadowMapSize
        , size_t capacity
    );

    // Prepares shadow maps for a specified number of lights. 
    // Calling this twice overwrites the previous results.
    void recordInitialize(
        VkCommandBuffer cmd
        , float depthBias
        , float depthBiasSlope
        , std::span<GPUTypes::LightDirectional const> directionalLights
        , std::span<GPUTypes::LightSpot const> spotLights
    );

    void recordDrawCommands(
        VkCommandBuffer cmd
        , MeshAsset const& mesh
        , TStagedBuffer<glm::mat4x4> const& models
    );

    // Transitions all the shadow map VkImages, with a total memory barrier.
    void recordTransitionActiveShadowMaps(
        VkCommandBuffer cmd
        , VkImageLayout dstLayout
    );

    VkDescriptorSetLayout samplerSetLayout() const 
    { 
        return m_samplerSetLayout; 
    };
    VkDescriptorSetLayout texturesSetLayout() const 
    { 
        return m_texturesSetLayout; 
    };
    VkDescriptorSet samplerSet() const 
    { 
        return m_samplerSet; 
    };
    VkDescriptorSet textureSet() const 
    { 
        return m_texturesSet; 
    };

    void cleanup(
        VkDevice const device
        , VmaAllocator const allocator
    )
    {
        for (auto& image : m_textures)
        {
            image.cleanup(device, allocator);
        }

        vkDestroySampler(device, m_sampler, nullptr);

        if (m_pipeline)
        {
            m_pipeline->cleanup(device);
        }

        vkDestroyDescriptorSetLayout(device, m_samplerSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, m_texturesSetLayout, nullptr);

        m_projViewMatrices.reset();

        m_textures.clear();
        m_pipeline.reset();
        m_sampler = VK_NULL_HANDLE;
        m_samplerSetLayout = VK_NULL_HANDLE;
        m_samplerSet = VK_NULL_HANDLE;
        m_texturesSetLayout = VK_NULL_HANDLE;
        m_texturesSet = VK_NULL_HANDLE;
        m_texturesCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

private:
    float m_depthBias{ 0 };
    float m_depthBiasSlope{ 0 };
    // Each of these staged values represents 
    // a shadow map we are going to write
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> m_projViewMatrices{};
    // The current layout of the textures, 
    // as recorded by this class.
    VkImageLayout m_texturesCurrentLayout{ VK_IMAGE_LAYOUT_UNDEFINED };

    VmaAllocator m_allocator{ VK_NULL_HANDLE };

    VkSampler m_sampler{ VK_NULL_HANDLE };
    VkDescriptorSetLayout m_samplerSetLayout{ VK_NULL_HANDLE };
    VkDescriptorSet m_samplerSet{ VK_NULL_HANDLE };

    std::vector<AllocatedImage> m_textures{};
    
    VkDescriptorSetLayout m_texturesSetLayout{ VK_NULL_HANDLE };
    VkDescriptorSet m_texturesSet{ VK_NULL_HANDLE };
    
    std::unique_ptr<OffscreenPassGraphicsPipeline> m_pipeline{};
};