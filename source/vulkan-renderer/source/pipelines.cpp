#include "pipelines.hpp"

#include <spirv_reflect.h>

#include <fstream>
#include "helpers.h"
#include "initializers.hpp"

void vkutil::printReflectionData(std::span<uint8_t> spirv_bytecode)
{
	SpvReflectShaderModule module;
	SpvReflectResult const result = spvReflectCreateShaderModule(
		spirv_bytecode.size_bytes(),
		spirv_bytecode.data(),
		&module
	);

	Log(fmt::format("Reflection: create module {}", static_cast<int32_t>(result)));

	uint32_t pushConstantCounts;
	spvReflectEnumeratePushConstants(
		&module,
		&pushConstantCounts,
		nullptr
	);
	std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantCounts);
	spvReflectEnumeratePushConstants(
		&module,
		&pushConstantCounts,
		pushConstants.data()
	);

	for (SpvReflectBlockVariable* const pPushConstant : pushConstants)
	{
		SpvReflectBlockVariable const& pushConstant{ *pPushConstant };

		Log(fmt::format("Reflection: pushConstant name \"{}\" with \"{}\" elements, byte size \"{}\"", 
			pushConstant.name, 
			pushConstant.member_count,
			pushConstant.size
		));

		std::span<SpvReflectBlockVariable> const members{ pushConstant.members, pushConstant.member_count };
		for (SpvReflectBlockVariable const& member : members)
		{
			Log(fmt::format("Reflection: pushConstant member name \"{}\" with \"{}\" elements", 
				member.name,
				member.member_count)
			);
		}
	}

	spvReflectDestroyShaderModule(&module);
}

bool vkutil::loadShaderModule(std::string const& localPath, VkDevice device, VkShaderModule* outShaderModule)
{
	Log(fmt::format("Compiling \"{}\"", localPath));
	std::unique_ptr<std::filesystem::path> const shaderPath = DebugUtils::getLoadedDebugUtils().loadAssetPath(std::filesystem::path(localPath));
	if (shaderPath == nullptr)
	{
		Error(fmt::format("Unable to get asset at \"{}\"", localPath));
		return false;
	}
	
	std::ifstream file(*shaderPath.get(), std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		Error(fmt::format("Unable to open shader at \"{}\"", localPath));
		return false;
	}

	size_t const fileSizeBytes = static_cast<size_t>(file.tellg());

	size_t const bufferLength = fileSizeBytes / sizeof(uint32_t);

	if (bufferLength == 0)
	{
		Error(fmt::format("Shader file is empty at \"{}\"", localPath));
	}

	std::vector<uint32_t> buffer(bufferLength);

	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char*>(buffer.data()), fileSizeBytes);

	file.close();

	vkutil::printReflectionData(std::span<uint8_t>(reinterpret_cast<uint8_t*>(buffer.data()), fileSizeBytes));

	VkShaderModuleCreateInfo const createInfo{
		.sType{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO },
		.pNext{ nullptr },

		.codeSize{ buffer.size() * sizeof(uint32_t) },
		.pCode{ buffer.data() },
	};

	VkResult const result{ vkCreateShaderModule(device, &createInfo, nullptr, outShaderModule) };
	LogVkResult(result, fmt::format("Compiled \"{}\"", localPath));
	if (result != VK_SUCCESS)
	{
		*outShaderModule = VK_NULL_HANDLE;
		return false;
	}

	return true;
}
