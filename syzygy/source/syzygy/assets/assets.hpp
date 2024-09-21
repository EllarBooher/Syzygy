#pragma once

#include "syzygy/assets/assetstypes.hpp"
#include "syzygy/core/uuid.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/material.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace syzygy
{
struct PlatformWindow;
struct UILayer;
struct GraphicsContext;
struct ImmediateSubmissionQueue;
struct ImageView;
struct ImageLoadingTask;
} // namespace syzygy

namespace syzygy
{
// An interval of indices from an index buffer.
struct GeometrySurface
{
    uint32_t firstIndex;
    uint32_t indexCount;
    MaterialData material{};
};

struct Mesh
{
    std::vector<GeometrySurface> surfaces{};
    AABB vertexBounds{};
    std::unique_ptr<syzygy::GPUMeshBuffers> meshBuffers{};
};

struct AssetFile
{
    std::filesystem::path path{};
    std::vector<uint8_t> fileBytes{};
};

auto loadAssetFile(std::filesystem::path const& path)
    -> std::optional<AssetFile>;

template <typename T>
auto assetPtrToRef(AssetPtr<T> const& asset) -> std::optional<AssetRef<T>>
{
    if (asset.lock() == nullptr)
    {
        return std::nullopt;
    }

    return *asset.lock();
}

struct AssetLibrary
{
public:
    template <typename T>
    [[nodiscard]] auto fetchAssets() -> std::vector<AssetPtr<T>>
    {
        std::vector<AssetPtr<T>> assets{};

        if constexpr (std::is_same_v<T, ImageView>)
        {
            assets.reserve(m_textures.size());
            for (auto& texture : m_textures)
            {
                assets.emplace_back(texture);
            }
        }
        else if constexpr (std::is_same_v<T, Mesh>)
        {
            assets.reserve(m_meshes.size());
            for (auto& texture : m_meshes)
            {
                assets.emplace_back(texture);
            }
        }

        return assets;
    }

    template <typename T>
    [[nodiscard]] auto fetchAssetRefs() -> std::vector<AssetRef<T>>
    {
        std::vector<AssetRef<T>> assets{};

        if constexpr (std::is_same_v<T, ImageView>)
        {
            assets.reserve(m_textures.size());
            for (auto& texture : m_textures)
            {
                if (texture == nullptr)
                {
                    continue;
                }
                assets.emplace_back(*texture);
            }
        }
        else if constexpr (std::is_same_v<T, Mesh>)
        {
            assets.reserve(m_meshes.size());
            for (auto& mesh : m_meshes)
            {
                if (mesh == nullptr)
                {
                    continue;
                }
                assets.emplace_back(*mesh);
            }
        }

        return assets;
    }

    // WARNING! This may invalidate existing references/pointers to assets
    template <typename T>
    auto registerAsset(
        std::shared_ptr<T> data,
        std::string const& name,
        std::optional<std::filesystem::path> const& sourcePath
    ) -> std::optional<AssetShared<T>>
    {
        Asset<T> asset{
            .metadata =
                AssetMetadata{
                    .displayName = deduplicateAssetName(name),
                    .id = UUID::createNew(),
                },
            .data = std::move(data),
        };

        if (sourcePath.has_value())
        {
            asset.metadata.fileLocalPath = sourcePath.value().string();
        }
        else
        {
            asset.metadata.fileLocalPath = "No source on disk.";
        }

        if constexpr (std::is_same_v<T, ImageView>)
        {
            m_textures.push_back(std::make_shared<Asset<T>>(std::move(asset)));
            return m_textures.back();
        }
        else if constexpr (std::is_same_v<T, Mesh>)
        {
            m_meshes.push_back(std::make_shared<Asset<T>>(std::move(asset)));
            return m_meshes.back();
        }

        return std::nullopt;
    }

    template <typename T> [[nodiscard]] auto empty() -> bool
    {
        if constexpr (std::is_same_v<T, ImageView>)
        {
            return m_textures.empty();
        }
        else if constexpr (std::is_same_v<T, Mesh>)
        {
            return m_meshes.empty();
        }

        return true;
    }

    auto loadTextureFromPath(
        VkDevice,
        VmaAllocator,
        VkQueue,
        ImmediateSubmissionQueue const&,
        VkFormat fileFormat,
        std::filesystem::path const& filePath
    ) -> std::optional<AssetShared<ImageView>>;

    void loadTexturesDialog(PlatformWindow const&, UILayer&);

    void loadGLTFFromPath(
        GraphicsContext&,
        ImmediateSubmissionQueue const&,
        std::filesystem::path const& filePath
    );

    void loadMeshesDialog(
        PlatformWindow const&,
        GraphicsContext&,
        ImmediateSubmissionQueue const& submissionQueue
    );

    static auto loadDefaultAssets(
        GraphicsContext&, ImmediateSubmissionQueue const& submissionQueue
    ) -> std::optional<AssetLibrary>;

    void processTasks(
        GraphicsContext&, ImmediateSubmissionQueue const& submissionQueue
    );

    enum class DefaultMeshAssets
    {
        Cube,
        Plane
    };

    auto defaultMesh(DefaultMeshAssets) -> AssetPtr<Mesh>;

private:
    AssetLibrary() = default;

    // Expects a name of format assetType_name and returns assetType_name_N
    // where N means there have been N-1 in existence
    // e.g. mesh_Cube becomes mesh_Cube_3
    auto deduplicateAssetName(std::string const& name) -> std::string;

    std::unordered_map<std::string, size_t> m_nameDuplicationCounters{};

    // The asset library stores pointers to all loaded/active assets, to keep
    // them alive for the lifetime of the library.
    // Asset library keeps shared_ptr<Asset<T>>, while the rest of the
    // application gets shared_ptr<Asset<T> const>. This allows interior
    // mutability of the asset data, but not metadata/pointers to data/metadata.

    AssetShared<ImageView> m_defaultColorMap{};
    AssetShared<ImageView> m_defaultNormalMap{};
    AssetShared<ImageView> m_defaultORMMap{};
    std::vector<std::shared_ptr<Asset<ImageView>>> m_textures{};

    AssetShared<Mesh> m_meshPlane{};
    AssetShared<Mesh> m_meshCube{};
    std::vector<std::shared_ptr<Asset<Mesh>>> m_meshes{};

    std::vector<std::shared_ptr<ImageLoadingTask>> m_tasks{};
};
} // namespace syzygy