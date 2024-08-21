#pragma once

#include "syzygy/core/uuid.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/image.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace syzygy
{
struct PlatformWindow;
struct GraphicsContext;
struct ImmediateSubmissionQueue;
} // namespace syzygy

namespace syzygy
{
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
    std::unique_ptr<syzygy::GPUMeshBuffers> meshBuffers{};
};

auto loadGltfMeshes(
    VkDevice,
    VmaAllocator,
    VkQueue transferQueue,
    syzygy::ImmediateSubmissionQueue const& submissionQueue,
    std::filesystem::path const& path
) -> std::optional<std::vector<std::shared_ptr<MeshAsset>>>;

struct AssetFile
{
    std::filesystem::path path{};
    std::vector<uint8_t> fileBytes{};
};

auto loadAssetFile(std::filesystem::path const& path)
    -> std::optional<AssetFile>;
struct AssetMetadata
{
    std::string displayName{};
    std::string fileLocalPath{};
    syzygy::UUID id{};
};

template <typename T> struct Asset
{
    AssetMetadata metadata{};
    std::shared_ptr<T> data{};
};

template <typename T> using AssetRef = std::reference_wrapper<Asset<T> const>;

class AssetLibrary
{
public:
    template <typename T>
    [[nodiscard]] auto fetchAssets() -> std::vector<AssetRef<T>>
    {
        std::vector<AssetRef<T>> assets{};

        if constexpr (std::is_same_v<T, Image>)
        {
            assets.reserve(m_textures.size());
            for (auto& texture : m_textures)
            {
                assets.emplace_back(texture);
            }
        }
        else if constexpr (std::is_same_v<T, MeshAsset>)
        {
            assets.reserve(m_meshes.size());
            for (auto& texture : m_meshes)
            {
                assets.emplace_back(texture);
            }
        }

        return assets;
    }

    template <typename T> [[nodiscard]] auto empty() -> bool
    {
        if constexpr (std::is_same_v<T, Image>)
        {
            return m_textures.empty();
        }
        else if constexpr (std::is_same_v<T, MeshAsset>)
        {
            return m_meshes.empty();
        }
    }

    void loadTexturesDialog(
        PlatformWindow const&,
        GraphicsContext&,
        ImmediateSubmissionQueue& submissionQueue
    );

    void loadMeshesDialog(
        PlatformWindow const&,
        GraphicsContext&,
        ImmediateSubmissionQueue& submissionQueue
    );

private:
    std::vector<Asset<Image>> m_textures{};
    std::vector<Asset<MeshAsset>> m_meshes{};
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
    VkQueue transferQueue,
    ImmediateSubmissionQueue const&,
    std::filesystem::path const& path,
    VkImageUsageFlags additionalFlags
) -> std::optional<Asset<syzygy::Image>>;
} // namespace syzygy