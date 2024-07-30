#pragma once

#include "syzygy/core/integer.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include "syzygy/vulkanusage.hpp"
#include <glm/vec2.hpp>
#include <imgui.h>
#include <memory>
#include <optional>
#include <span>
#include <string>

struct MeshAsset;
enum class RenderingPipelines;

template <typename T>
void imguiStructureControls(T& structure, T const& defaultStructure);

template <typename T> void imguiStructureControls(T& structure);

template <typename T> void imguiStructureDisplay(T const& structure);

void imguiRenderingSelection(RenderingPipelines& currentActivePipeline);