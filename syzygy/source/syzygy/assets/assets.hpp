#pragma once

#include "syzygy/core/uuid.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/material.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace syzygy
{
struct PlatformWindow;
struct GraphicsContext;
struct ImmediateSubmissionQueue;
struct ImageView;
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

// TODO: ID should act as a unique key into a table of metadatas, instead of
// duplicating this data everywhere AssetMetadata is passed
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

    template <typename T> [[nodiscard]] auto empty() -> bool
    {
        if constexpr (std::is_same_v<T, Image>)
        {
            return m_textures.empty();
        }
        else if constexpr (std::is_same_v<T, Mesh>)
        {
            return m_meshes.empty();
        }

        return true;
    }

    // TODO: make this a member that adds the texture to the library
    static auto loadTextureFromPath(
        VkDevice,
        VmaAllocator,
        VkQueue,
        ImmediateSubmissionQueue const&,
        std::filesystem::path const& filePath,
        VkImageUsageFlags additionalFlags
    ) -> std::optional<syzygy::Asset<syzygy::Image>>;

    void loadTexturesDialog(
        PlatformWindow const&,
        GraphicsContext&,
        ImmediateSubmissionQueue const& submissionQueue
    );

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

private:
    AssetLibrary() = default;

    // Expects a name of format assetType_name and returns assetType_name_N
    // where N means there have been N-1 in existence
    // e.g. mesh_Cube becomes mesh_Cube_3
    auto deduplicateAssetName(std::string const& name) -> std::string;

    std::unordered_map<std::string, size_t> m_nameDuplicationCounters{};

    std::vector<Asset<Image>> m_textures{};

    size_t m_defaultColorIndex{0};
    size_t m_defaultNormalIndex{0};
    size_t m_defaultORMIndex{0};
    std::vector<Asset<ImageView>> m_imageViews{};

    std::vector<Asset<Mesh>> m_meshes{};
};
} // namespace syzygy