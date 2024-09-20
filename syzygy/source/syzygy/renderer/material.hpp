#pragma once

#include "syzygy/assets/assetstypes.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include <memory>
#include <optional>

namespace syzygy
{
struct DescriptorAllocator;
struct ImageView;
} // namespace syzygy

namespace syzygy
{
struct MaterialData
{
    // Occlusion Roughness Metallic texture, stored RGB in that respective order
    AssetPtr<ImageView> ORM{};
    AssetPtr<ImageView> normal{};
    AssetPtr<ImageView> color{};
};

struct MaterialDescriptors
{
public:
    auto operator=(MaterialDescriptors&&) -> MaterialDescriptors& = delete;
    MaterialDescriptors(MaterialDescriptors const&) = delete;
    auto operator=(MaterialDescriptors const&) -> MaterialDescriptors& = delete;

    MaterialDescriptors(MaterialDescriptors&&) noexcept;
    ~MaterialDescriptors();

private:
    MaterialDescriptors() = default;
    void destroy();

public:
    static auto create(VkDevice, DescriptorAllocator&)
        -> std::optional<MaterialDescriptors>;

    // Public methods go here

    void write(MaterialData const&) const;
    void bind(VkCommandBuffer, VkPipelineLayout, uint32_t colorSet) const;

private:
    // Member variables go here
    VkDevice m_device{VK_NULL_HANDLE};

    // These are allocated from some outside pool

    VkSampler m_sampler{};

    VkDescriptorSetLayout m_colorLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_colorSet{VK_NULL_HANDLE};
};
} // namespace syzygy