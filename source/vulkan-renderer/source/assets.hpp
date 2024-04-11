#pragma once

#include "engine_types.h"
#include <optional>
#include <filesystem>
#include "buffers.hpp"

/** An interval of indices from an index buffer. */
struct GeometrySurface
{
    uint32_t firstIndex;
    uint32_t indexCount;
};

struct MeshAsset {
    std::string name{};
    std::vector<GeometrySurface> surfaces{};
    std::unique_ptr<GPUMeshBuffers> meshBuffers{};
};

class Engine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Engine* engine, std::string localPath);