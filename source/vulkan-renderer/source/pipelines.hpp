#pragma once

#include "engine_types.h"
#include "shaders.hpp"

namespace vkutil
{
	ShaderWrapper loadShaderModule(std::string const& localPath, VkDevice device);
}