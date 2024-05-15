#pragma once

#include <span>
#include <memory>

#include "../pipelines.hpp"

struct MeshAsset;

template<typename T>
void imguiStructureControls(T& structure, T const& defaultValues);

template<typename T>
void imguiStructureControls(T& structure);

template<typename T>
void imguiStructureDisplay(T const& structure);

void imguiMeshInstanceControls(
    bool& shouldRender,
    std::span<std::shared_ptr<MeshAsset> const> meshes,
    size_t& meshIndex
);

void imguiRenderingSelection(
    RenderingPipelines& currentActivePipeline
);

void imguiPerformanceWindow(
    std::span<double const> fpsValues, 
    double averageFPS, 
    size_t currentFrame,
    float& targetFPS
);