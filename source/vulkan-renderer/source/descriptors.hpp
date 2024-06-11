#pragma once

#include "enginetypes.hpp"
#include <optional>

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
    DescriptorLayoutBuilder&
    addBinding(AddBindingParameters parameters, uint32_t count);

    // Adds an additional binding that will be built. Infers the count from the
    // length of samplers.
    DescriptorLayoutBuilder& addBinding(
        AddBindingParameters parameters, std::vector<VkSampler> samplers
    );

    void clear();

    std::optional<VkDescriptorSetLayout>
    build(VkDevice device, VkDescriptorSetLayoutCreateFlags layoutFlags) const;

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
        float ratio{0.0f};
    };

    VkDescriptorPool getPool() const { return pool; }

    void initPool(
        VkDevice device,
        uint32_t maxSets,
        std::span<PoolSizeRatio const> poolRatios,
        VkDescriptorPoolCreateFlags flags
    );
    void clearDescriptors(VkDevice device);
    void destroyPool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);

private:
    VkDescriptorPool pool{VK_NULL_HANDLE};
};
