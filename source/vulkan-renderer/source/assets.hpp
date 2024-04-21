#pragma once

#include "enginetypes.hpp"
#include <optional>
#include <filesystem>
#include <variant>
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

AssetLoadingResult loadAssetFile(std::string const& localPath, VkDevice device);