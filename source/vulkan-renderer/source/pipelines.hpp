#pragma once

#include "engine_types.h"

namespace vkutil
{
	void printReflectionData(std::span<uint8_t> spirv_bytecode);
	bool loadShaderModule(std::string const& localPath, VkDevice device, VkShaderModule* outShaderModule);
}