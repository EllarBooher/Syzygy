#pragma once

#include "engine_types.h"

struct DescriptorLayoutBuilder
{
	/**
	Saves a VkDescriptorSetLayoutBinding that defaults to having one element, with no pipeline stage access.
	*/
	DescriptorLayoutBuilder& addBinding(
		uint32_t binding, 
		VkDescriptorType type, 
		VkShaderStageFlags stageMask,
		uint32_t count,
		VkDescriptorBindingFlags flags
	);
	void clear();
	
	/**
	@param shaderStages Additional shader stages to add to every binding.
	@returns The newly allocated set.
	*/
	VkDescriptorSetLayout build(VkDevice device, VkDescriptorSetLayoutCreateFlags flags) const;

private:
	std::vector<VkDescriptorSetLayoutBinding> m_bindings;
	std::vector<VkDescriptorBindingFlags> m_flags;
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
