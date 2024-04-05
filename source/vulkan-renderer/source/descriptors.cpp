#include "descriptors.hpp"
#include "helpers.h"

DescriptorLayoutBuilder& DescriptorLayoutBuilder::addBinding(
	uint32_t binding, 
	VkDescriptorType type, 
	VkShaderStageFlags stageMask,
	uint32_t count,
	VkDescriptorBindingFlags flags
)
{
	VkDescriptorSetLayoutBinding const newBinding{
		.binding{ binding },
		.descriptorType{ type },
		.descriptorCount{ count },
		.stageFlags{ stageMask },
		.pImmutableSamplers{ nullptr }
	};

	m_flags.push_back(flags);
	m_bindings.push_back(newBinding);

	return *this;
}

void DescriptorLayoutBuilder::clear()
{
	m_flags.clear();
	m_bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(
	VkDevice device, 
	VkDescriptorSetLayoutCreateFlags flags
) const
{
	VkDescriptorSetLayoutBindingFlagsCreateInfo const flagsInfo{
		.sType{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO },
		.pNext{ nullptr },

		.bindingCount{ static_cast<uint32_t>(m_flags.size()) },
		.pBindingFlags{ m_flags.data() },
	};

	VkDescriptorSetLayoutCreateInfo const info{
		.sType{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO },
		.pNext{ &flagsInfo },
		.flags{ flags },
		.bindingCount{ static_cast<uint32_t>(m_bindings.size()) },
		.pBindings{ m_bindings.data() },
	};

	VkDescriptorSetLayout set;
	CheckVkResult(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}

void DescriptorAllocator::initPool(
	VkDevice device, 
	uint32_t maxSets, 
	std::span<PoolSizeRatio const> poolRatios,
	VkDescriptorPoolCreateFlags flags
)
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
		.flags{ flags },
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
