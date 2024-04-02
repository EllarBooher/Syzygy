#include "pipelines.hpp"

#include <fstream>
#include "helpers.h"
#include "initializers.hpp"
#include "shaders.hpp"

ShaderWrapper vkutil::loadShaderModule(std::string const& localPath, VkDevice device)
{
	Log(fmt::format("Compiling \"{}\"", localPath));
	std::unique_ptr<std::filesystem::path> const pShaderPath = DebugUtils::getLoadedDebugUtils().loadAssetPath(std::filesystem::path(localPath));
	if (pShaderPath == nullptr)
	{
		Error(fmt::format("Unable to get asset at \"{}\"", localPath));
		return ShaderWrapper::Invalid();
	}

	std::filesystem::path const shaderPath = *pShaderPath.get();
	
	std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		Error(fmt::format("Unable to open shader at \"{}\"", localPath));
		return ShaderWrapper::Invalid();
	}

	size_t const fileSizeBytes = static_cast<size_t>(file.tellg());
	if (fileSizeBytes == 0)
	{
		Error(fmt::format("Shader file is empty at \"{}\"", localPath));
		return ShaderWrapper::Invalid();
	}

	std::vector<uint8_t> buffer(fileSizeBytes);

	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char*>(buffer.data()), fileSizeBytes);

	file.close();

	std::string const shaderName = shaderPath.filename().string();
	return ShaderWrapper::FromBytecode(device, shaderName, buffer);
}
