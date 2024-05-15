#pragma once

#include "enginetypes.hpp"
#include <optional>

struct DescriptorLayoutBuilder
{
	DescriptorLayoutBuilder& addBinding(
		uint32_t binding
		, VkDescriptorType type
		, VkShaderStageFlags stageMask
		, uint32_t count
		, VkDescriptorBindingFlags flags
	);

	DescriptorLayoutBuilder& addBinding(
		uint32_t binding
		, VkDescriptorType type
		, VkShaderStageFlags stageMask
		, std::span<VkSampler const> samplers
		, VkDescriptorBindingFlags flags
	);

	DescriptorLayoutBuilder& addBinding(
		uint32_t binding
		, VkDescriptorType type
		, VkShaderStageFlags stageMask
		, VkSampler sampler
		, VkDescriptorBindingFlags flags
	);

	void clear();

	std::optional<VkDescriptorSetLayout> build(
		VkDevice device
		, VkDescriptorSetLayoutCreateFlags layoutFlags
	) const;

private:
	struct Binding
	{
		std::vector<VkSampler> immutableSamplers{};
		VkDescriptorSetLayoutBinding binding{};
		VkDescriptorBindingFlags flags{};
	};

	std::vector<Binding> m_bindings{};
};

/**
A struct that holds a descriptor pool and allows allocating from it.
*/
class DescriptorAllocator
{
public:
	struct PoolSizeRatio {
		VkDescriptorType type{ VK_DESCRIPTOR_TYPE_SAMPLER };
		float ratio{ 0.0f };
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
	VkDescriptorPool pool{ VK_NULL_HANDLE };
};
