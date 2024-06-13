#pragma once

#include "buffers.hpp"
#include "enginetypes.hpp"
#include <filesystem>
#include <optional>
#include <variant>

/** An interval of indices from an index buffer. */
struct GeometrySurface
{
    uint32_t firstIndex;
    uint32_t indexCount;
};

struct MeshAsset
{
    std::string name{};
    std::vector<GeometrySurface> surfaces{};
    std::unique_ptr<GPUMeshBuffers> meshBuffers{};
};

class Engine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>>
loadGltfMeshes(Engine* engine, std::string const& localPath);

struct AssetFile
{
    std::string fileName{};
    std::vector<uint8_t> fileBytes{};
};

struct AssetLoadingError
{
    std::string message{};
};

using AssetLoadingResult = std::variant<AssetFile, AssetLoadingError>;

AssetLoadingResult loadAssetFile(std::string const& localPath);