#include "descriptors.hpp"
#include "helpers.h"

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageMask)
{
	VkDescriptorSetLayoutBinding const newBinding{
		.binding{ binding },
		.descriptorType{ type },
		.descriptorCount{ 1 },
		.stageFlags{ stageMask },
		.pImmutableSamplers{ nullptr }
	};

	bindings.push_back(newBinding);
}

void DescriptorLayoutBuilder::clear()
{
	bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags stageMask)
{
	std::vector<VkDescriptorSetLayoutBinding> outBindings{};

	// Copy to in-place modify parameters
	for (VkDescriptorSetLayoutBinding binding : bindings)
	{
		binding.stageFlags |= stageMask;
		outBindings.push_back(binding);
	}

	VkDescriptorSetLayoutCreateInfo const info{
		.sType{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO },
		.pNext{ nullptr },
		.flags{ 0 },
		.bindingCount{ static_cast<uint32_t>(outBindings.size()) },
		.pBindings{ outBindings.data() },
	};

	VkDescriptorSetLayout set;
	CheckVkResult(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}

void DescriptorAllocator::initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio const> poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes{};
	for (PoolSizeRatio const& ratio : poolRatios)
	{
		auto const descriptorCount = static_cast<uint32_t>(roundf(ratio.ratio * maxSets));

		poolSizes.push_back(VkDescriptorPoolSize{
			.type{ ratio.type },
			.descriptorCount{ descriptorCount },
		});
	}

	VkDescriptorPoolCreateInfo const poolInfo{
		.sType{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO },
		.flags{ 0 },
		.maxSets{ maxSets },
		.poolSizeCount{ static_cast<uint32_t>(poolSizes.size()) },
		.pPoolSizes{ poolSizes.data() },
	};

	CheckVkResult(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool));
}

void DescriptorAllocator::clearDescriptors(VkDevice device)
{
	CheckVkResult(vkResetDescriptorPool(device, pool, 0));
}

void DescriptorAllocator::destroyPool(VkDevice device)
{
	vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
	VkDescriptorSetAllocateInfo const allocInfo{
		.sType{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO },
		.pNext{ nullptr },
		.descriptorPool{ pool },
		.descriptorSetCount{ 1 },
		.pSetLayouts{ &layout }
	};

	VkDescriptorSet set;
	CheckVkResult(vkAllocateDescriptorSets(device, &allocInfo, &set));

	return set;
}
