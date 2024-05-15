#include "descriptors.hpp"
#include "helpers.hpp"

DescriptorLayoutBuilder& DescriptorLayoutBuilder::addBinding(
	uint32_t const binding
	, VkDescriptorType const type
	, VkShaderStageFlags const stageMask
	, uint32_t const count
	, VkDescriptorBindingFlags const flags
)
{
	VkDescriptorSetLayoutBinding const newBinding{
		.binding{ binding },
		.descriptorType{ type },
		.descriptorCount{ count },
		.stageFlags{ stageMask },
		.pImmutableSamplers{ nullptr }
	};

	m_bindings.push_back(Binding{
		.binding{ newBinding },
		.flags{ flags }
	});

	return *this;
}

DescriptorLayoutBuilder& DescriptorLayoutBuilder::addBinding(
	uint32_t const binding
	, VkDescriptorType const type
	, VkShaderStageFlags const stageMask
	, std::span<VkSampler const> samplers
	, VkDescriptorBindingFlags const flags
)
{
	// We leave data uninitialized until layout creation time to avoid self referencing.
	VkDescriptorSetLayoutBinding const newBinding{
		.binding{ binding },
		.descriptorType{ type },
		.descriptorCount{ 0 },
		.stageFlags{ stageMask },
		.pImmutableSamplers{ nullptr }
	};

	m_bindings.push_back(
		Binding{
			.immutableSamplers{ samplers.begin(), samplers.end() },
			.binding{ newBinding },
			.flags{ flags },
		}
	);

	return *this;
}

DescriptorLayoutBuilder& DescriptorLayoutBuilder::addBinding(
	uint32_t const binding
	, VkDescriptorType const type
	, VkShaderStageFlags const stageMask
	, VkSampler const sampler
	, VkDescriptorBindingFlags const flags
)
{
	std::array<VkSampler, 1> samplers{ sampler };
	return addBinding(
		binding
		, type
		, stageMask
		, samplers
		, flags
	);
}

void DescriptorLayoutBuilder::clear()
{
	m_bindings.clear();
}

std::optional<VkDescriptorSetLayout> DescriptorLayoutBuilder::build(
	VkDevice device
	, VkDescriptorSetLayoutCreateFlags layoutFlags
) const
{
	std::vector<VkDescriptorSetLayoutBinding> bindings{};
	std::vector<VkDescriptorBindingFlags> bindingFlags{};

	for (Binding const& inBinding : m_bindings)
	{
		VkDescriptorSetLayoutBinding binding{ inBinding.binding };

		if (inBinding.immutableSamplers.size() > 0)
		{
			binding.descriptorCount = inBinding.immutableSamplers.size();
			binding.pImmutableSamplers = inBinding.immutableSamplers.data();
		}
		
		bindings.push_back(binding);
		bindingFlags.push_back(inBinding.flags);
	}

	VkDescriptorSetLayoutBindingFlagsCreateInfo const flagsInfo{
		.sType{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO },
		.pNext{ nullptr },

		.bindingCount{ static_cast<uint32_t>(bindingFlags.size()) },
		.pBindingFlags{ bindingFlags.data() },
	};

	VkDescriptorSetLayoutCreateInfo const info{
		.sType{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO },
		.pNext{ &flagsInfo },
		.flags{ layoutFlags },
		.bindingCount{ static_cast<uint32_t>(bindings.size()) },
		.pBindings{ bindings.data() },
	};

	VkDescriptorSetLayout set;
	VkResult const result{
		vkCreateDescriptorSetLayout(device, &info, nullptr, &set)
	};
	if (result != VK_SUCCESS)
	{
		LogVkResult(result, "Creating Descriptor Set Layout");
		return {};
	}

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
