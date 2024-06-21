#pragma once

#include "../descriptors.hpp"
#include "../enginetypes.hpp"
#include "../images.hpp"

struct GBuffer
{
    GBuffer() = default;

    GBuffer(GBuffer&& other) { *this = std::move(other); }
    GBuffer& operator=(GBuffer&& other)
    {
        diffuseColor.swap(other.diffuseColor);
        specularColor.swap(other.specularColor);
        normal.swap(other.normal);
        worldPosition.swap(other.worldPosition);

        descriptorLayout =
            std::exchange(other.descriptorLayout, VK_NULL_HANDLE);
        descriptors = std::exchange(other.descriptors, VK_NULL_HANDLE);

        immutableSamplers = std::move(other.immutableSamplers);

        return *this;
    }

    GBuffer(GBuffer const& other) = delete;
    GBuffer& operator=(GBuffer const& other) = delete;

    GBuffer(
        AllocatedImage&& diffuse,
        AllocatedImage&& specular,
        AllocatedImage&& normal,
        AllocatedImage&& worldPosition,
        VkDescriptorSetLayout descriptorLayout,
        VkDescriptorSet descriptors,
        std::span<VkSampler const> immutableSamplers
    )
        : diffuseColor{std::make_unique<AllocatedImage>(std::move(diffuse))}
        , specularColor{std::make_unique<AllocatedImage>(std::move(specular))}
        , normal{std::make_unique<AllocatedImage>(std::move(normal))}
        , worldPosition{std::make_unique<AllocatedImage>(std::move(worldPosition
          ))}
        , descriptorLayout{descriptorLayout}
        , descriptors{descriptors}
        , immutableSamplers{immutableSamplers.begin(), immutableSamplers.end()}
    {
    }

    std::unique_ptr<AllocatedImage> diffuseColor{};
    std::unique_ptr<AllocatedImage> specularColor{};
    std::unique_ptr<AllocatedImage> normal{};
    std::unique_ptr<AllocatedImage> worldPosition{};

    VkDescriptorSetLayout descriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSet descriptors{VK_NULL_HANDLE};

    // We keep these since their handles are baked into descriptors
    std::vector<VkSampler> immutableSamplers{};

    static std::optional<GBuffer> create(
        VkDevice device,
        VkExtent2D drawExtent,
        VmaAllocator allocator,
        DescriptorAllocator& descriptorAllocator
    );

    VkExtent2D extent() const { return diffuseColor->extent2D(); }

    void recordTransitionImages(
        VkCommandBuffer cmd, VkImageLayout srcLayout, VkImageLayout dstLayout
    ) const;

    void cleanup(VkDevice device, VmaAllocator allocator);
};