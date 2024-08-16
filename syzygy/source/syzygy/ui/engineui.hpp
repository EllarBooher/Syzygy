#pragma once

namespace syzygy
{
enum class RenderingPipelines;

template <typename T>
void imguiStructureControls(T& structure, T const& defaultStructure);

template <typename T> void imguiStructureControls(T& structure);

template <typename T> void imguiStructureDisplay(T const& structure);

void imguiRenderingSelection(RenderingPipelines& currentActivePipeline);
} // namespace syzygy