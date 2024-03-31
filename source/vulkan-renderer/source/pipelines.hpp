#pragma once

#include "engine_types.h"

namespace vkutil
{
	bool loadShaderModule(std::string const& localPath, VkDevice device, VkShaderModule* outShaderModule);
}