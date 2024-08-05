#pragma once

#include "syzygy/buffers.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/core/uuid.hpp"
#include "syzygy/vulkanusage.hpp"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace szg_image
{
struct Image;
}
struct ImmediateSubmissionQueue;

// An interval of indices from an index buffer.
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

struct MeshAssetLibrary
{
    std::vector<std::shared_ptr<MeshAsset>> loadedMeshes;
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(
    VkDevice,
    VmaAllocator,
    VkQueue transferQueue,
    ImmediateSubmissionQueue const& submissionQueue,
    std::string const& localPath
);

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

namespace szg_assets
{
struct AssetMetadata
{
    std::string displayName{};
    std::string fileLocalPath{};
    szg::UUID id{};
};

template <typename T> struct Asset
{
    AssetMetadata metadata{};
    std::unique_ptr<T> data{};
};

template <typename T> using AssetRef = std::reference_wrapper<Asset<T> const>;

class AssetLibrary
{
public:
    void registerAsset(Asset<szg_image::Image>&& asset);
    auto fetchAssets() -> std::vector<AssetRef<szg_image::Image>>;

private:
    std::vector<Asset<szg_image::Image>> m_textures{};
};

struct ImageRGBA
{
    uint32_t x{0};
    uint32_t y{0};
    std::vector<uint8_t> bytes{};
};
auto loadTextureFromFile(
    VkDevice,
    VmaAllocator,
    VkQueue const transferQueue,
    ImmediateSubmissionQueue const&,
    std::string const& localPath,
    VkImageUsageFlags const additionalFlags
) -> std::optional<Asset<szg_image::Image>>;
} // namespace szg_assets