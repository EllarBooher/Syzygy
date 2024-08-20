#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include <optional>
#include <span>
#include <vector>

namespace syzygy
{
struct DescriptorLayoutBuilder
{
    struct AddBindingParameters
    {
        uint32_t binding;
        VkDescriptorType type;
        VkShaderStageFlags stageMask;
        VkDescriptorBindingFlags bindingFlags;
    };

    // Adds an additional binding that will be built.
    auto addBinding(AddBindingParameters parameters, uint32_t count)
        -> DescriptorLayoutBuilder&;

    // Adds an additional binding that will be built. Infers the count from the
    // length of samplers.
    auto
    addBinding(AddBindingParameters parameters, std::vector<VkSampler> samplers)
        -> DescriptorLayoutBuilder&;

    void clear();

    auto
    build(VkDevice device, VkDescriptorSetLayoutCreateFlags layoutFlags) const
        -> std::optional<VkDescriptorSetLayout>;

private:
    struct Binding
    {
        std::vector<VkSampler> immutableSamplers{};
        VkDescriptorSetLayoutBinding binding{};
        VkDescriptorBindingFlags flags{};
    };

    std::vector<Binding> m_bindings{};
};

// Holds a descriptor pool and allows allocating from it.
class DescriptorAllocator
{
public:
    struct PoolSizeRatio
    {
        VkDescriptorType type{VK_DESCRIPTOR_TYPE_SAMPLER};
        float ratio{0.0F};
    };

    DescriptorAllocator() = delete;

    DescriptorAllocator(DescriptorAllocator const&) = delete;
    auto operator=(DescriptorAllocator const&) -> DescriptorAllocator& = delete;

    DescriptorAllocator(DescriptorAllocator&&) noexcept;
    auto operator=(DescriptorAllocator&&) noexcept -> DescriptorAllocator&;

    ~DescriptorAllocator();

    static auto create(
        VkDevice device,
        uint32_t maxSets,
        std::span<PoolSizeRatio const> poolRatios,
        VkDescriptorPoolCreateFlags flags
    ) -> DescriptorAllocator;
    void clearDescriptors(VkDevice device);

    auto allocate(VkDevice device, VkDescriptorSetLayout layout)
        -> VkDescriptorSet;

private:
    DescriptorAllocator(VkDevice device, VkDescriptorPool pool)
        : m_device{device}
        , m_pool{pool}
    {
    }

    void destroy() noexcept;

    VkDevice m_device{VK_NULL_HANDLE};
    VkDescriptorPool m_pool{VK_NULL_HANDLE};
};
} // namespace syzygy