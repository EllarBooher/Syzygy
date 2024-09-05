#pragma once

#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/imageview.hpp"

namespace syzygy
{
class DescriptorAllocator;
struct MaterialData;

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

struct MaterialData
{
    // Occlusion Roughness Metallic texture, stored RGB in that respective order
    std::shared_ptr<ImageView> ORM{};
    std::shared_ptr<ImageView> normal{};
    std::shared_ptr<ImageView> color{};
};

} // namespace syzygy