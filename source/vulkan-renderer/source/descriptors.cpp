#include "descriptors.hpp"
#include "helpers.hpp"

auto DescriptorLayoutBuilder::addBinding(
    AddBindingParameters const parameters,
    uint32_t const count
) -> DescriptorLayoutBuilder &
{
	VkDescriptorSetLayoutBinding const newBinding{
		.binding = parameters.binding,
		.descriptorType = parameters.type,
		.descriptorCount = count,
		.stageFlags = parameters.stageMask,
		.pImmutableSamplers = nullptr,
	};

	m_bindings.push_back(
		Binding{
			.binding = newBinding,
			.flags = parameters.bindingFlags,
		}
	);

	return *this;
}

auto DescriptorLayoutBuilder::addBinding(
	AddBindingParameters const parameters,
    std::vector<VkSampler> samplers
) -> DescriptorLayoutBuilder &
{
	// We leave data uninitialized until layout creation time.
	// This keeps the immutable samplers in alive in a managed container,
	// while avoiding carrying self-referential pointers here.
	VkDescriptorSetLayoutBinding const newBinding{
		.binding = parameters.binding,
		.descriptorType = parameters.type,
		.descriptorCount = 0,
		.stageFlags = parameters.stageMask,
		.pImmutableSamplers = nullptr,
	};

	m_bindings.push_back(
		Binding{
			.immutableSamplers = std::move(samplers),
			.binding = newBinding,
			.flags = parameters.bindingFlags,
		}
	);

	return *this;
}

void DescriptorLayoutBuilder::clear()
{
	m_bindings.clear();
}

auto DescriptorLayoutBuilder::build(VkDevice const device, VkDescriptorSetLayoutCreateFlags const layoutFlags) const
    -> std::optional<VkDescriptorSetLayout>
{
	std::vector<VkDescriptorSetLayoutBinding> bindings{};
	std::vector<VkDescriptorBindingFlags> bindingFlags{};

	for (Binding const& inBinding : m_bindings)
	{
		VkDescriptorSetLayoutBinding binding{ inBinding.binding };

        if (!inBinding.immutableSamplers.empty())
        {
			binding.descriptorCount = inBinding.immutableSamplers.size();
			binding.pImmutableSamplers = inBinding.immutableSamplers.data();
		}
		
		bindings.push_back(binding);
		bindingFlags.push_back(inBinding.flags);
	}

	VkDescriptorSetLayoutBindingFlagsCreateInfo const flagsInfo{
		.sType = 
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO 
		,
		.pNext = nullptr,

		.bindingCount = static_cast<uint32_t>(bindingFlags.size()),
		.pBindingFlags = bindingFlags.data(),
	};

	VkDescriptorSetLayoutCreateInfo const info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &flagsInfo,
		.flags = layoutFlags,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data(),
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
	VkDevice const device
	, uint32_t const maxSets
	, std::span<PoolSizeRatio const> const poolRatios
	, VkDescriptorPoolCreateFlags const flags
)
{
	std::vector<VkDescriptorPoolSize> poolSizes{};
	for (PoolSizeRatio const& ratio : poolRatios)
	{
		auto const descriptorCount{
			static_cast<uint32_t>(roundf(ratio.ratio * static_cast<float>(maxSets)))
		};

		poolSizes.push_back(
			VkDescriptorPoolSize{
				.type = ratio.type,
				.descriptorCount = descriptorCount,
			}
		);
	}

	VkDescriptorPoolCreateInfo const poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = flags,
		.maxSets = maxSets,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};

	CheckVkResult(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool));
}

void DescriptorAllocator::clearDescriptors(VkDevice const device)
{
	CheckVkResult(vkResetDescriptorPool(device, pool, 0));
}

void DescriptorAllocator::destroyPool(VkDevice const device)
{
	vkDestroyDescriptorPool(device, pool, nullptr);
}

auto DescriptorAllocator::allocate(VkDevice const device, VkDescriptorSetLayout const layout) -> VkDescriptorSet
{
	VkDescriptorSetAllocateInfo const allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	VkDescriptorSet set;
	CheckVkResult(vkAllocateDescriptorSets(device, &allocInfo, &set));

	return set;
}
