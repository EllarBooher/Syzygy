#include "pipelines.hpp"

#include <fstream>
#include "helpers.h"
#include "initializers.hpp"

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
