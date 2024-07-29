#pragma once

#include "syzygy/buffers.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/images.hpp"
#include "syzygy/pipelines.hpp"
#include "syzygy/vulkanusage.hpp"
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <span>
#include <vector>

class DescriptorAllocator;
namespace gputypes
{
struct LightDirectional;
struct LightSpot;
} // namespace gputypes
struct MeshAsset;

struct ShadowPassParameters
{
    float depthBiasConstant{2.00f};
    float depthBiasSlope{-1.75f};
};

// Handles the resources for an array of depth maps,
// which share a sampler and should be accessed via a descriptor array.
// TODO: Implement move/copy/constructors to properly manage resources
class ShadowPassArray
{
public:
    static size_t constexpr SHADOWPASS_CAMERA_CAPACITY{100};

    static std::optional<ShadowPassArray> create(
        VkDevice device,
        DescriptorAllocator& descriptorAllocator,
        VmaAllocator allocator,
        VkExtent2D shadowmapExtent,
        size_t capacity
    );

    // Prepares shadow maps for a specified number of lights.
    // Calling this twice overwrites the previous results.
    void recordInitialize(
        VkCommandBuffer cmd,
        ShadowPassParameters parameters,
        std::span<gputypes::LightDirectional const> directionalLights,
        std::span<gputypes::LightSpot const> spotLights
    );

    void recordDrawCommands(
        VkCommandBuffer cmd,
        MeshAsset const& mesh,
        TStagedBuffer<glm::mat4x4> const& models
    );

    // Transitions all the shadow map VkImages, with a total memory barrier.
    void recordTransitionActiveShadowMaps(
        VkCommandBuffer cmd, VkImageLayout dstLayout
    );

    VkDescriptorSetLayout samplerSetLayout() const
    {
        return m_samplerSetLayout;
    };
    VkDescriptorSetLayout texturesSetLayout() const
    {
        return m_texturesSetLayout;
    };
    VkDescriptorSet samplerSet() const { return m_samplerSet; };
    VkDescriptorSet textureSet() const { return m_texturesSet; };

    void cleanup(VkDevice const device, VmaAllocator const allocator)
    {
        for (auto& image : m_textures)
        {
            image.reset();
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
    }

private:
    float m_depthBias{0};
    float m_depthBiasSlope{0};
    // Each of these staged values represents
    // a shadow map we are going to write
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> m_projViewMatrices{};

    VmaAllocator m_allocator{VK_NULL_HANDLE};

    VkSampler m_sampler{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_samplerSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_samplerSet{VK_NULL_HANDLE};

    std::vector<std::unique_ptr<AllocatedImage>> m_textures{};

    VkDescriptorSetLayout m_texturesSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_texturesSet{VK_NULL_HANDLE};

    std::unique_ptr<OffscreenPassGraphicsPipeline> m_pipeline{};
};