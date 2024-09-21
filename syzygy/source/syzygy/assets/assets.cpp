#include "assets.hpp"

#include "syzygy/core/immediate.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/editor/graphicscontext.hpp"
#include "syzygy/platform/filesystemutils.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/platformutils.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/ui/uiwidgets.hpp"
#include <algorithm>
#include <cassert>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp> // IWYU pragma: keep
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <limits>
#include <span>
#include <spdlog/fmt/bundled/core.h>
#include <utility>
#include <variant>
#include <vulkan/vk_enum_string_helper.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace
{
struct RGBATexel
{
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{std::numeric_limits<uint8_t>::max()};

    static uint8_t constexpr SATURATED_COMPONENT{255U};
};

struct ImageRGBA
{
    uint32_t x{0};
    uint32_t y{0};
    std::vector<uint8_t> bytes{};
};
} // namespace

namespace detail
{
auto uploadImageToGPU(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    syzygy::ImmediateSubmissionQueue const& submissionQueue,
    VkFormat const format,
    VkImageUsageFlags const additionalFlags,
    ImageRGBA const& image
) -> std::optional<std::unique_ptr<syzygy::Image>>
{
    VkExtent2D const imageExtent{.width = image.x, .height = image.y};

    std::optional<std::unique_ptr<syzygy::Image>> stagingImageResult{
        syzygy::Image::allocate(
            device,
            allocator,
            syzygy::ImageAllocationParameters{
                .extent = imageExtent,
                .format = format,
                .usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
                .tiling = VK_IMAGE_TILING_LINEAR,
                .vmaUsage = VMA_MEMORY_USAGE_CPU_ONLY,
                .vmaFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
            }
        )
    };
    if (!stagingImageResult.has_value())
    {
        SZG_ERROR("Failed to allocate staging image.");
        return std::nullopt;
    }
    syzygy::Image& stagingImage{*stagingImageResult.value()};

    std::optional<VmaAllocationInfo> const allocationInfo{
        stagingImage.fetchAllocationInfo()
    };

    if (allocationInfo.has_value()
        && allocationInfo.value().pMappedData != nullptr)
    {
        auto* const stagingImageData{
            reinterpret_cast<uint8_t*>(allocationInfo.value().pMappedData)
        };

        std::copy(image.bytes.begin(), image.bytes.end(), stagingImageData);
    }
    else
    {
        SZG_ERROR("Failed to map bytes of staging image.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<syzygy::Image>> finalImageResult{
        syzygy::Image::allocate(
            device,
            allocator,
            syzygy::ImageAllocationParameters{
                .extent = imageExtent,
                .format = format,
                .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT | additionalFlags,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .tiling = VK_IMAGE_TILING_OPTIMAL
            }
        )
    };
    if (!finalImageResult.has_value())
    {
        SZG_ERROR("Failed to allocate final image.");
        return std::nullopt;
    }
    syzygy::Image& finalImage{*finalImageResult.value()};

    if (auto const submissionResult{submissionQueue.immediateSubmit(
            transferQueue,
            [&](VkCommandBuffer const cmd)
    {
        stagingImage.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
        );

        finalImage.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
        );

        syzygy::Image::recordCopyEntire(
            cmd, stagingImage, finalImage, VK_IMAGE_ASPECT_COLOR_BIT
        );
    }
        )};
        submissionResult
        != syzygy::ImmediateSubmissionQueue::SubmissionResult::SUCCESS)
    {
        SZG_ERROR("Failed to copy images.");
        return std::nullopt;
    }

    return std::move(finalImageResult).value();
}

auto uploadMeshToGPU(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    syzygy::ImmediateSubmissionQueue const& submissionQueue,
    std::span<uint32_t const> const indices,
    std::span<syzygy::VertexPacked const> const vertices
) -> std::unique_ptr<syzygy::GPUMeshBuffers>
{
    // Allocate buffer

    size_t const indexBufferSize{indices.size_bytes()};
    size_t const vertexBufferSize{vertices.size_bytes()};

    syzygy::AllocatedBuffer indexBuffer{syzygy::AllocatedBuffer::allocate(
        device,
        allocator,
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    )};

    syzygy::AllocatedBuffer vertexBuffer{syzygy::AllocatedBuffer::allocate(
        device,
        allocator,
        vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    )};

    // Copy data into buffer

    syzygy::AllocatedBuffer stagingBuffer{syzygy::AllocatedBuffer::allocate(
        device,
        allocator,
        vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VMA_ALLOCATION_CREATE_MAPPED_BIT
    )};

    assert(
        stagingBuffer.isMapped()
        && "Staging buffer for mesh upload was not mapped."
    );

    stagingBuffer.writeBytes(
        0,
        std::span<uint8_t const>{
            reinterpret_cast<uint8_t const*>(vertices.data()), vertexBufferSize
        }
    );
    stagingBuffer.writeBytes(
        vertexBufferSize,
        std::span<uint8_t const>{
            reinterpret_cast<uint8_t const*>(indices.data()), indexBufferSize
        }
    );

    if (auto result{submissionQueue.immediateSubmit(
            transferQueue,
            [&](VkCommandBuffer cmd)
    {
        VkBufferCopy const vertexCopy{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = vertexBufferSize,
        };
        vkCmdCopyBuffer(
            cmd, stagingBuffer.buffer(), vertexBuffer.buffer(), 1, &vertexCopy
        );

        VkBufferCopy const indexCopy{
            .srcOffset = vertexBufferSize,
            .dstOffset = 0,
            .size = indexBufferSize,
        };
        vkCmdCopyBuffer(
            cmd, stagingBuffer.buffer(), indexBuffer.buffer(), 1, &indexCopy
        );
    }
        )};
        result != syzygy::ImmediateSubmissionQueue::SubmissionResult::SUCCESS)
    {
        SZG_WARNING("Command submission for mesh upload failed, buffers will "
                    "likely contain junk or no data.");
    }

    return std::make_unique<syzygy::GPUMeshBuffers>(
        std::move(indexBuffer), std::move(vertexBuffer)
    );
}

auto registerTextureFromRGBA(
    syzygy::AssetLibrary& library,
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    syzygy::ImmediateSubmissionQueue const& submissionQueue,
    VkFormat const format,
    std::string const& name,
    ImageRGBA const& image,
    std::optional<std::filesystem::path> const& sourcePath
) -> std::optional<syzygy::AssetShared<syzygy::ImageView>>
{
    // TODO: add more formats and a way to generally check if a format is
    // reasonable. Also support copying 32 bit -> any image format.
    if (format != VK_FORMAT_R8G8B8A8_UNORM && format != VK_FORMAT_R8G8B8A8_SRGB)
    {
        SZG_WARNING(
            "Uploading texture to device as possibly unsupported format '{}'- "
            "images are loaded onto the CPU as 32 bit RGBA.",
            string_VkFormat(format)
        );
    }

    std::optional<std::unique_ptr<syzygy::Image>> uploadResult{uploadImageToGPU(
        device,
        allocator,
        transferQueue,
        submissionQueue,
        format,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        image
    )};
    if (!uploadResult.has_value())
    {
        SZG_ERROR("Failed to upload image to GPU.");
        return std::nullopt;
    }
    std::optional<std::unique_ptr<syzygy::ImageView>> imageViewResult{
        syzygy::ImageView::allocate(
            device,
            allocator,
            std::move(*uploadResult.value()),
            syzygy::ImageViewAllocationParameters{}
        )
    };
    if (!imageViewResult.has_value() || imageViewResult.value() == nullptr)
    {
        SZG_ERROR("Failed to convert image into imageview.");
        return std::nullopt;
    }

    return library.registerAsset<syzygy::ImageView>(
        std::move(imageViewResult).value(),
        fmt::format("texture_{}", name),
        sourcePath
    );
}

} // namespace detail

namespace detail_stbi
{
auto loadRGBA(std::span<uint8_t const> const bytes) -> std::optional<ImageRGBA>
{
    int32_t x{0};
    int32_t y{0};

    int32_t components{0};
    uint16_t constexpr RGBA_COMPONENT_COUNT{4};

    stbi_uc* const parsedImage{stbi_load_from_memory(
        bytes.data(),
        static_cast<int32_t>(bytes.size()),
        &x,
        &y,
        &components,
        RGBA_COMPONENT_COUNT
    )};

    if (parsedImage == nullptr)
    {
        SZG_ERROR("stbi: Failed to convert image.");
        return std::nullopt;
    }

    if (x < 1 || y < 1)
    {
        SZG_ERROR(fmt::format(
            "stbi: Parsed image had invalid dimensions: ({},{})", x, y
        ));
        return std::nullopt;
    }

    auto widthPixels{static_cast<uint32_t>(x)};
    auto heightPixels{static_cast<uint32_t>(y)};
    size_t constexpr BYTES_PER_COMPONENT{1};
    size_t constexpr BYTES_PER_PIXEL{
        RGBA_COMPONENT_COUNT * BYTES_PER_COMPONENT
    };
    std::vector<uint8_t> const rgba{
        parsedImage,
        parsedImage
            + static_cast<size_t>(widthPixels * heightPixels) * BYTES_PER_PIXEL
    };

    stbi_image_free(parsedImage);

    return ImageRGBA{.x = widthPixels, .y = heightPixels, .bytes = rgba};
}
} // namespace detail_stbi

namespace detail_fastgltf
{
struct ImageChannelOverrides
{
    std::optional<uint8_t> red{};
    std::optional<uint8_t> green{};
    std::optional<uint8_t> blue{};
    std::optional<uint8_t> alpha{};
};

// glTF material texture indices oragnized into syzygy's material format
struct MaterialTextureIndices
{
    std::optional<size_t> color{};
    std::optional<size_t> normal{};
    std::optional<size_t> occlusion{};
    std::optional<size_t> roughnessMetallic{};
};

enum class MapTypes
{
    Color,
    Normal,
    OcclusionRoughnessMetallic
};
auto string_MapTypes(MapTypes const mapType)
{
    switch (mapType)
    {
    case MapTypes::Color:
        return "color";
    case MapTypes::Normal:
        return "normal";
    case MapTypes::OcclusionRoughnessMetallic:
        return "orm";
    }
}

auto loadGLTFAsset(std::filesystem::path const& path)
    -> fastgltf::Expected<fastgltf::Asset>
{
    std::filesystem::path const assetPath{syzygy::ensureAbsolutePath(path)};

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(assetPath);

    auto constexpr GLTF_OPTIONS{
        fastgltf::Options::LoadGLBBuffers
        | fastgltf::Options::LoadExternalBuffers
        //        | fastgltf::Options::LoadExternalImages // Defer loading
        //        images so we have access to the URIs
    };

    fastgltf::Parser parser{};

    if (assetPath.extension() == ".gltf")
    {
        return parser.loadGltfJson(
            &data, assetPath.parent_path(), GLTF_OPTIONS
        );
    }

    return parser.loadGltfBinary(&data, assetPath.parent_path(), GLTF_OPTIONS);
}

// Preserves glTF indexing.
auto getTextureSources(
    std::span<fastgltf::Texture const> const textures,
    std::span<fastgltf::Image const> const images
) -> std::vector<std::optional<std::reference_wrapper<fastgltf::Image const>>>
{
    std::vector<std::optional<std::reference_wrapper<fastgltf::Image const>>>
        textureSourcesByGLTFIndex{};
    textureSourcesByGLTFIndex.reserve(textures.size());
    for (fastgltf::Texture const& texture : textures)
    {
        textureSourcesByGLTFIndex.emplace_back(std::nullopt);
        auto& sourceImage{textureSourcesByGLTFIndex.back()};

        if (!texture.imageIndex.has_value())
        {
            SZG_WARNING("Texture {} was missing imageIndex.", texture.name);
            continue;
        }

        size_t const loadedIndex{texture.imageIndex.value()};

        if (loadedIndex >= textures.size())
        {
            SZG_WARNING(
                "Texture {} had imageIndex that was out of bounds.",
                texture.name
            );
            continue;
        }

        sourceImage = images[loadedIndex];
    }

    return textureSourcesByGLTFIndex;
}

auto convertGLTFImageToRGBAAndFullyQualifiedSource(
    fastgltf::Image const& image,
    ImageChannelOverrides const overrides,
    std::filesystem::path const& assetRoot
) -> std::optional<std::tuple<ImageRGBA, std::filesystem::path>>
{
    std::optional<std::tuple<ImageRGBA, std::filesystem::path>> result{
        std::nullopt
    };

    if (std::holds_alternative<fastgltf::sources::Array>(image.data))
    {
        std::span<uint8_t const> const data =
            std::get<fastgltf::sources::Array>(image.data).bytes;

        std::optional<ImageRGBA> imageConvertResult{detail_stbi::loadRGBA(data)
        };

        if (imageConvertResult.has_value())
        {
            result = {std::move(imageConvertResult.value()), assetRoot};
        }
    }
    else if (std::holds_alternative<fastgltf::sources::URI>(image.data))
    {
        fastgltf::sources::URI const& uri{
            std::get<fastgltf::sources::URI>(image.data)
        };

        // These asserts should be loosened as we support a larger subset of
        // glTF.
        assert(uri.fileByteOffset == 0);
        assert(uri.uri.isLocalPath());

        std::filesystem::path const path{assetRoot / uri.uri.fspath()};

        std::ifstream file(path, std::ios::binary);

        if (!std::filesystem::is_regular_file(path))
        {
            SZG_WARNING(
                "glTF image source URI does not result in a valid file path. "
                "URI was: {}. Full path is: {}",
                uri.uri.string(),
                path.string()
            );
            return std::nullopt;
        }

        size_t const lengthBytes{std::filesystem::file_size(path)};

        std::vector<uint8_t> data(lengthBytes);
        file.read(
            reinterpret_cast<char*>(data.data()),
            static_cast<std::streamsize>(lengthBytes)
        );

        // Throw the file to stbi and hope for the best, it should detect the
        // file headers properly

        std::optional<ImageRGBA> imageConvertResult{detail_stbi::loadRGBA(data)
        };

        if (imageConvertResult.has_value())
        {
            result = {std::move(imageConvertResult.value()), path};
        }
    }
    else
    {
        SZG_WARNING("Unsupported glTF image source found.");
        return std::nullopt;
    }

    if (!result.has_value())
    {
        SZG_WARNING("Failed to load image from glTF.");
        return std::nullopt;
    }

    uint64_t const redSelector{overrides.red.has_value() ? 0U : 1U};
    uint64_t const redValue{overrides.red.value_or(0)};

    uint64_t const greenSelector{overrides.green.has_value() ? 0U : 1U};
    uint64_t const greenValue{overrides.green.value_or(0)};

    uint64_t const blueSelector{overrides.blue.has_value() ? 0U : 1U};
    uint64_t const blueValue{overrides.blue.value_or(0)};

    uint64_t const alphaSelector{overrides.alpha.has_value() ? 0U : 1U};
    uint64_t const alphaValue{overrides.alpha.value_or(0)};

    auto& imageBytes{std::get<0>(result.value()).bytes};
    for (RGBATexel& texel : std::span<RGBATexel>{
             reinterpret_cast<RGBATexel*>(imageBytes.data()),
             imageBytes.size() / sizeof(RGBATexel)
         })
    {
        texel.r = static_cast<uint8_t>(texel.r * redSelector + redValue);
        texel.g = static_cast<uint8_t>(texel.g * greenSelector + greenValue);
        texel.b = static_cast<uint8_t>(texel.b * blueSelector + blueValue);
        texel.a = static_cast<uint8_t>(texel.a * alphaSelector + alphaValue);
    }

    return result;
}

// Preserves gltf indexing. Returns a vector whose size matches the count of
// materials passed in.
auto parseMaterialIndices(fastgltf::Material const& material)
    -> MaterialTextureIndices
{
    MaterialTextureIndices indices{};

    {
        std::optional<fastgltf::TextureInfo> const& color{
            material.pbrData.baseColorTexture
        };
        if (!color.has_value())
        {
            SZG_WARNING("Material {}: Missing color texture.", material.name);
        }
        else
        {
            indices.color = color.value().textureIndex;
        }
    }

    {
        std::optional<fastgltf::NormalTextureInfo> const& normal{
            material.normalTexture
        };
        if (!normal.has_value())
        {
            SZG_WARNING("Material {}: Missing normal texture.", material.name);
        }
        else
        {
            indices.normal = normal.value().textureIndex;
        }
    }

    {
        std::optional<fastgltf::OcclusionTextureInfo> const& occlusion{
            material.occlusionTexture
        };
        if (!occlusion.has_value())
        {
            SZG_WARNING(
                "Material {}: Missing occlusion texture.", material.name
            );
        }
        else
        {
            indices.occlusion = occlusion.value().textureIndex;
        }
    }

    {
        std::optional<fastgltf::TextureInfo> const& metallicRoughness{
            material.pbrData.metallicRoughnessTexture
        };
        if (!metallicRoughness.has_value())
        {
            SZG_WARNING(
                "Material {}: Missing metallicRoughness texture", material.name
            );
        }
        else
        {
            indices.roughnessMetallic = metallicRoughness.value().textureIndex;
        }
    }

    return indices;
}

auto accessTexture(
    std::span<std::optional<std::reference_wrapper<fastgltf::Image const>>>
        textureSourcesByGLTFIndex,
    size_t const textureIndex
) -> std::optional<std::reference_wrapper<fastgltf::Image const>>
{
    if (textureIndex >= textureSourcesByGLTFIndex.size())
    {
        SZG_WARNING("Out of bounds texture index.");
        return std::nullopt;
    }

    if (!textureSourcesByGLTFIndex[textureIndex].has_value())
    {
        SZG_WARNING("Texture index source was not loaded.");
        return std::nullopt;
    }

    return textureSourcesByGLTFIndex[textureIndex].value().get();
}

auto uploadTextureFromIndex(
    syzygy::AssetLibrary& destinationLibrary,
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    syzygy::ImmediateSubmissionQueue const& submissionQueue,
    std::span<std::optional<std::reference_wrapper<fastgltf::Image const>>>
        textureSourcesByGLTFIndex,
    size_t const textureIndex,
    ImageChannelOverrides const overrides,
    std::filesystem::path const& assetRoot,
    std::string const& gltfAssetName,
    MapTypes const mapType
) -> std::optional<syzygy::AssetShared<syzygy::ImageView>>
{
    std::optional<std::reference_wrapper<fastgltf::Image const>> textureResult{
        accessTexture(textureSourcesByGLTFIndex, textureIndex)
    };
    if (!textureResult.has_value())
    {
        return std::nullopt;
    }

    std::optional<std::tuple<ImageRGBA, std::filesystem::path>> convertResult{
        convertGLTFImageToRGBAAndFullyQualifiedSource(
            textureResult.value().get(), overrides, assetRoot
        )
    };
    if (!convertResult.has_value())
    {
        return std::nullopt;
    }

    VkFormat fileFormat{};
    switch (mapType)
    {
    case MapTypes::Color:
        fileFormat = VK_FORMAT_R8G8B8A8_SRGB;
        break;
    case MapTypes::Normal:
    case MapTypes::OcclusionRoughnessMetallic:
        fileFormat = VK_FORMAT_R8G8B8A8_UNORM;
        break;
    }

    std::string assetName{textureResult.value().get().name};
    if (assetName.empty())
    {
        assetName = fmt::format(
            "{}_{}_{}", gltfAssetName, textureIndex, string_MapTypes(mapType)
        );
    }

    return detail::registerTextureFromRGBA(
        destinationLibrary,
        device,
        allocator,
        transferQueue,
        submissionQueue,
        fileFormat,
        assetName,
        std::get<0>(convertResult.value()),
        std::get<1>(convertResult.value())
    );
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto uploadMaterialDataAsAssets(
    syzygy::AssetLibrary& destinationLibrary,
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    syzygy::ImmediateSubmissionQueue const& submissionQueue,
    syzygy::MaterialData const& fallbackMaterialData,
    fastgltf::Asset const& gltf,
    std::filesystem::path const& assetRoot
) -> std::vector<syzygy::MaterialData>
{
    // Follow texture.imageIndex -> image indirection by one step
    std::vector<std::optional<std::reference_wrapper<fastgltf::Image const>>>
        textureSourcesByGLTFIndex{
            detail_fastgltf::getTextureSources(gltf.textures, gltf.images)
        };

    std::vector<syzygy::MaterialData> materialDataByGLTFIndex{};
    materialDataByGLTFIndex.reserve(gltf.materials.size());
    for (fastgltf::Material const& material : gltf.materials)
    {
        materialDataByGLTFIndex.push_back(fallbackMaterialData);
        syzygy::MaterialData& materialData{materialDataByGLTFIndex.back()};

        detail_fastgltf::MaterialTextureIndices materialTextures{
            parseMaterialIndices(material)
        };

        if (materialTextures.roughnessMetallic.has_value()
            || materialTextures.occlusion.has_value())
        {
            size_t ormTextureIndex{};
            ImageChannelOverrides overrides{};

            if (materialTextures.occlusion.has_value()
                && materialTextures.occlusion
                       != materialTextures.roughnessMetallic)
            {
                SZG_WARNING("Material {}: occlusion and roughnessMetallic "
                            "textures differ. Loading roughnessMetallic and "
                            "overriding its occlusion channel.");
            }

            if (materialTextures.roughnessMetallic.has_value())
            {
                ormTextureIndex = materialTextures.roughnessMetallic.value();
                overrides.red = RGBATexel::SATURATED_COMPONENT;
            }
            else
            {
                ormTextureIndex = materialTextures.occlusion.value();
                overrides.green = 0U;
                overrides.blue = 0U;
            }

            if (std::optional<syzygy::AssetShared<syzygy::ImageView>>
                    textureLoadResult{uploadTextureFromIndex(
                        destinationLibrary,
                        device,
                        allocator,
                        transferQueue,
                        submissionQueue,
                        textureSourcesByGLTFIndex,
                        ormTextureIndex,
                        overrides,
                        assetRoot,
                        std::string{material.name},
                        MapTypes::OcclusionRoughnessMetallic
                    )};
                !textureLoadResult.has_value()
                || textureLoadResult.value() == nullptr)
            {
                SZG_WARNING(
                    "Material {}: Failed to upload ORM texture.", material.name
                );
            }
            else
            {
                materialData.ORM = textureLoadResult.value();
            }
        }

        if (materialTextures.color.has_value())
        {
            if (std::optional<syzygy::AssetShared<syzygy::ImageView>>
                    textureLoadResult{uploadTextureFromIndex(
                        destinationLibrary,
                        device,
                        allocator,
                        transferQueue,
                        submissionQueue,
                        textureSourcesByGLTFIndex,
                        materialTextures.color.value(),
                        {},
                        assetRoot,
                        std::string{material.name},
                        MapTypes::Color
                    )};
                !textureLoadResult.has_value()
                || textureLoadResult.value() == nullptr)
            {
                SZG_WARNING(
                    "Material {}: Failed to upload color texture.",
                    material.name
                );
            }
            else
            {
                materialData.color = textureLoadResult.value();
            }
        }

        if (materialTextures.normal.has_value())
        {
            if (std::optional<syzygy::AssetShared<syzygy::ImageView>>
                    textureLoadResult{uploadTextureFromIndex(
                        destinationLibrary,
                        device,
                        allocator,
                        transferQueue,
                        submissionQueue,
                        textureSourcesByGLTFIndex,
                        materialTextures.normal.value(),
                        {},
                        assetRoot,
                        std::string{material.name},
                        MapTypes::Normal
                    )};
                !textureLoadResult.has_value()
                || textureLoadResult.value() == nullptr)
            {
                SZG_WARNING(
                    "Material {}: Failed to upload normal texture.",
                    material.name
                );
            }
            else
            {
                materialData.normal = textureLoadResult.value();
            }
        }
    }

    return materialDataByGLTFIndex;
}

// Preserves gltf indexing, with nullptr on any positions where loading
// failed. All passed gltf objects should come from the same object, so
// accessors are utilized properly.
// TODO: simplify and breakup. There are some roadblocks because e.g. fastgltf
// accessors are separate from the mesh
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto loadMeshes(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    syzygy::ImmediateSubmissionQueue const& submissionQueue,
    std::span<syzygy::MaterialData const> const materialsByGLTFIndex,
    syzygy::MaterialData const& defaultMaterial,
    fastgltf::Asset const& gltf
) -> std::vector<std::unique_ptr<syzygy::Mesh>>
{
    std::vector<std::unique_ptr<syzygy::Mesh>> newMeshes{};
    newMeshes.reserve(gltf.meshes.size());
    for (fastgltf::Mesh const& mesh : gltf.meshes)
    {
        newMeshes.push_back(nullptr);
        std::unique_ptr<syzygy::Mesh>& newMesh{newMeshes.back()};

        std::vector<uint32_t> indices{};
        std::vector<syzygy::VertexPacked> vertices{};

        std::vector<syzygy::GeometrySurface> surfaces{};

        // Proliferate indices and vertices
        for (auto&& primitive : mesh.primitives)
        {
            if (!primitive.indicesAccessor.has_value()
                || primitive.indicesAccessor.value() >= gltf.accessors.size())
            {
                SZG_WARNING("glTF mesh primitive had no valid indices "
                            "accessor. It will be skipped.");
                continue;
            }
            if (auto const* positionAttribute{primitive.findAttribute("POSITION"
                )};
                positionAttribute == primitive.attributes.end()
                || positionAttribute == nullptr)
            {
                SZG_WARNING("glTF mesh primitive had no valid vertices "
                            "accessor. It will be skipped.");
                continue;
            }

            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                SZG_WARNING("Loading glTF mesh primitive as Triangles mode "
                            "when it is not.");
            }

            surfaces.push_back(syzygy::GeometrySurface{
                .firstIndex = static_cast<uint32_t>(indices.size()),
                .indexCount = 0,
                .material = defaultMaterial
            });
            syzygy::GeometrySurface& surface{surfaces.back()};

            if (!primitive.materialIndex.has_value())
            {
                SZG_WARNING(
                    "Mesh {} has a primitive that is missing material "
                    "index.",
                    mesh.name
                );
            }
            else if (size_t const materialIndex{primitive.materialIndex.value()
                     };
                     materialIndex >= materialsByGLTFIndex.size())
            {
                SZG_WARNING(
                    "Mesh {} has a primitive with out of bounds material "
                    "index.",
                    mesh.name
                );
            }
            else
            {
                surface.material = materialsByGLTFIndex[materialIndex];
            }

            size_t const initialVertexIndex{vertices.size()};

            { // Indices, not optional
                fastgltf::Accessor const& indicesAccessor{
                    gltf.accessors[primitive.indicesAccessor.value()]
                };

                surface.indexCount =
                    static_cast<uint32_t>(indicesAccessor.count);

                indices.reserve(indices.size() + indicesAccessor.count);
                fastgltf::iterateAccessor<uint32_t>(
                    gltf,
                    indicesAccessor,
                    [&](uint32_t index)
                { indices.push_back(index + initialVertexIndex); }
                );
            }

            { // Positions, not optional
                fastgltf::Accessor const& positionAccessor{
                    gltf.accessors[primitive.findAttribute("POSITION")->second]
                };

                vertices.reserve(vertices.size() + positionAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    gltf,
                    positionAccessor,
                    [&](glm::vec3 position, size_t /*index*/)
                {
                    vertices.push_back(syzygy::VertexPacked{
                        .position = position,
                        .uv_x = 0.0F,
                        .normal = glm::vec3{1, 0, 0},
                        .uv_y = 0.0F,
                        .color = glm::vec4{1.0F},
                    });
                }
                );
            }

            // The rest of these parameters are optional.

            { // Normals
                auto const* const normals{primitive.findAttribute("NORMAL")};
                if (normals != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        gltf,
                        gltf.accessors[(*normals).second],
                        [&](glm::vec3 normal, size_t index)
                    { vertices[initialVertexIndex + index].normal = normal; }
                    );
                }
            }

            { // UVs
                auto const* const uvs{primitive.findAttribute("TEXCOORD_0")};
                if (uvs != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        gltf,
                        gltf.accessors[(*uvs).second],
                        [&](glm::vec2 texcoord, size_t index)
                    {
                        vertices[initialVertexIndex + index].uv_x = texcoord.x;
                        vertices[initialVertexIndex + index].uv_y = texcoord.y;
                    }
                    );
                }
            }

            { // Colors
                auto const* const colors{primitive.findAttribute("COLOR_0")};
                if (colors != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        gltf,
                        gltf.accessors[(*colors).second],
                        [&](glm::vec4 color, size_t index)
                    { vertices[initialVertexIndex + index].color = color; }
                    );
                }
            }
        }

        bool constexpr FLIP_Y{true};
        if (FLIP_Y)
        {
            for (syzygy::VertexPacked& vertex : vertices)
            {
                vertex.normal.y *= -1;
                vertex.position.y *= -1;
            }
        }

        if (surfaces.empty())
        {
            continue;
        }

        glm::vec3 vertexMinimum{std::numeric_limits<float>::max()};
        glm::vec3 vertexMaximum{std::numeric_limits<float>::lowest()};

        for (syzygy::VertexPacked const& vertex : vertices)
        {
            vertexMinimum = glm::min(vertex.position, vertexMinimum);
            vertexMaximum = glm::max(vertex.position, vertexMaximum);
        }

        newMesh = std::make_unique<syzygy::Mesh>(syzygy::Mesh{
            .surfaces = std::move(surfaces),
            .vertexBounds = syzygy::AABB::create(vertexMinimum, vertexMaximum),
            .meshBuffers = detail::uploadMeshToGPU(
                device,
                allocator,
                transferQueue,
                submissionQueue,
                indices,
                vertices
            ),
        });
    }

    return newMeshes;
}
} // namespace detail_fastgltf

namespace syzygy
{
auto loadAssetFile(std::filesystem::path const& path)
    -> std::optional<AssetFile>
{
    std::filesystem::path const assetPath{syzygy::ensureAbsolutePath(path)};
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        SZG_ERROR("Unable to open file at {}", path.string());
        return std::nullopt;
    }

    size_t const fileSizeBytes = static_cast<size_t>(file.tellg());
    if (fileSizeBytes == 0)
    {
        SZG_ERROR("File at empty at {}", path.string());
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(fileSizeBytes);

    file.seekg(0, std::ios::beg);
    file.read(
        reinterpret_cast<char*>(buffer.data()),
        static_cast<std::streamsize>(fileSizeBytes)
    );

    file.close();

    return AssetFile{
        .path = path,
        .fileBytes = buffer,
    };
}

auto AssetLibrary::loadTextureFromPath(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    ImmediateSubmissionQueue const& submissionQueue,
    VkFormat const fileFormat,
    std::filesystem::path const& filePath
) -> std::optional<AssetShared<ImageView>>
{
    SZG_INFO("Loading Texture from '{}'", filePath.string());
    std::optional<AssetFile> const fileResult{loadAssetFile(filePath)};
    if (!fileResult.has_value())
    {
        SZG_ERROR("Failed to open file for texture.");
        return std::nullopt;
    }

    AssetFile const& file{fileResult.value()};

    std::optional<ImageRGBA> imageResult{detail_stbi::loadRGBA(file.fileBytes)};
    if (!imageResult.has_value())
    {
        SZG_ERROR("Failed to convert file to 32 bit RGBA image.");
        return std::nullopt;
    }

    return detail::registerTextureFromRGBA(
        *this,
        device,
        allocator,
        transferQueue,
        submissionQueue,
        fileFormat,
        file.path.stem().string(),
        imageResult.value(),
        filePath
    );
}

void AssetLibrary::loadTexturesDialog(
    PlatformWindow const& window, UILayer& uiLayer
)
{
    auto const paths{openFiles(window)};
    if (paths.empty())
    {
        return;
    }

    std::shared_ptr<ImageLoadingTask> loadingTask{
        ImageLoaderWidget::create(uiLayer, paths)
    };

    if (loadingTask == nullptr)
    {
        SZG_ERROR("Failed to create image loading task.");
        return;
    }
    m_tasks.push_back(std::move(loadingTask));
}

void AssetLibrary::loadGLTFFromPath(
    GraphicsContext& graphicsContext,
    ImmediateSubmissionQueue const& submissionQueue,
    std::filesystem::path const& filePath
)
{
    SZG_INFO("Loading glTF from {}", filePath.string());

    fastgltf::Expected<fastgltf::Asset> gltfLoadResult{
        detail_fastgltf::loadGLTFAsset(filePath)
    };
    if (gltfLoadResult.error() != fastgltf::Error::None)
    {
        SZG_ERROR(fmt::format(
            "Failed to load glTF: {} : {}",
            fastgltf::getErrorName(gltfLoadResult.error()),
            fastgltf::getErrorMessage(gltfLoadResult.error())
        ));
        return;
    }
    fastgltf::Asset const& gltf{gltfLoadResult.get()};

    MaterialData const defaultMaterialData{
        .ORM = m_defaultORMMap,
        .normal = m_defaultNormalMap,
        .color = m_defaultColorMap,
    };

    std::vector<MaterialData> const materialDataByGLTFIndex{
        detail_fastgltf::uploadMaterialDataAsAssets(
            *this,
            graphicsContext.device(),
            graphicsContext.allocator(),
            graphicsContext.universalQueue(),
            submissionQueue,
            defaultMaterialData,
            gltf,
            filePath.parent_path()
        )
    };

    std::vector<std::unique_ptr<syzygy::Mesh>> newMeshes{
        detail_fastgltf::loadMeshes(
            graphicsContext.device(),
            graphicsContext.allocator(),
            graphicsContext.universalQueue(),
            submissionQueue,
            materialDataByGLTFIndex,
            defaultMaterialData,
            gltf
        )
    };

    size_t loadedMeshes{0};
    for (size_t gltfMeshIndex{0}; gltfMeshIndex < newMeshes.size();
         gltfMeshIndex++)
    {
        if (newMeshes[gltfMeshIndex] == nullptr)
        {
            continue;
        }

        if (registerAsset<Mesh>(
                std::move(newMeshes[gltfMeshIndex]),
                fmt::format("mesh_{}", gltf.meshes[gltfMeshIndex].name),
                filePath
            )
                .has_value())
        {
            loadedMeshes++;
        }
    }

    SZG_INFO("Loaded {} meshes from glTF", loadedMeshes);
}

void AssetLibrary::loadMeshesDialog(
    PlatformWindow const& window,
    GraphicsContext& graphicsContext,
    ImmediateSubmissionQueue const& submissionQueue
)
{
    auto const paths{openFiles(window)};
    if (paths.empty())
    {
        return;
    }

    for (auto const& path : paths)
    {
        loadGLTFFromPath(graphicsContext, submissionQueue, path);
    }
}

auto AssetLibrary::loadDefaultAssets(
    GraphicsContext& graphicsContext,
    ImmediateSubmissionQueue const& submissionQueue
) -> std::optional<AssetLibrary>
{
    std::optional<AssetLibrary> libraryResult{AssetLibrary{}};
    AssetLibrary& library{libraryResult.value()};

    size_t constexpr DEFAULT_IMAGE_DIMENSIONS{64ULL};

    ImageRGBA defaultImage{
        .x = DEFAULT_IMAGE_DIMENSIONS,
        .y = DEFAULT_IMAGE_DIMENSIONS,
        .bytes = std::vector<uint8_t>{}
    };
    defaultImage.bytes.resize(
        static_cast<size_t>(defaultImage.x)
        * static_cast<size_t>(defaultImage.y) * sizeof(RGBATexel)
    );
    {
        for (RGBATexel& texel : std::span<RGBATexel>{
                 reinterpret_cast<RGBATexel*>(defaultImage.bytes.data()),
                 defaultImage.bytes.size() / sizeof(RGBATexel)
             })
        {
            RGBATexel constexpr NON_OCCLUDED_DIALECTRIC{
                .r = 255U, .g = 60U, .b = 0U, .a = 0U
            };

            texel = NON_OCCLUDED_DIALECTRIC;
        }

        library.m_defaultORMMap = detail::registerTextureFromRGBA(
                                      library,
                                      graphicsContext.device(),
                                      graphicsContext.allocator(),
                                      graphicsContext.universalQueue(),
                                      submissionQueue,
                                      VK_FORMAT_R8G8B8A8_UNORM,
                                      "NonOccludedDialectric",
                                      defaultImage,
                                      std::nullopt
        )
                                      .value();
    }
    {
        // Default color texture is a grey checkerboard

        size_t index{0};
        for (RGBATexel& texel : std::span<RGBATexel>{
                 reinterpret_cast<RGBATexel*>(defaultImage.bytes.data()),
                 defaultImage.bytes.size() / sizeof(RGBATexel)
             })
        {
            size_t const x{index % defaultImage.x};
            size_t const y{index / defaultImage.x};

            RGBATexel constexpr LIGHT_GREY{
                .r = 200U, .g = 200U, .b = 200U, .a = 255U
            };
            RGBATexel constexpr DARK_GREY{
                .r = 100U, .g = 100U, .b = 100U, .a = 255U
            };

            bool const lightSquare{((x / 4) + (y / 4)) % 2 == 0};

            texel = lightSquare ? LIGHT_GREY : DARK_GREY;

            index++;
        }

        library.m_defaultColorMap = detail::registerTextureFromRGBA(
                                        library,
                                        graphicsContext.device(),
                                        graphicsContext.allocator(),
                                        graphicsContext.universalQueue(),
                                        submissionQueue,
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        "defaultColor",
                                        defaultImage,
                                        std::nullopt
        )
                                        .value();
    }
    {
        // Default normal texture

        for (RGBATexel& texel : std::span<RGBATexel>{
                 reinterpret_cast<RGBATexel*>(defaultImage.bytes.data()),
                 defaultImage.bytes.size() / sizeof(RGBATexel)
             })
        {
            // Signed normal of (0,0,1) stored as unsigned (0.5,0.5,1.0)
            RGBATexel constexpr DEFAULT_NORMAL{
                .r = 127U, .g = 127U, .b = 255U, .a = 0U
            };

            texel = DEFAULT_NORMAL;
        }

        library.m_defaultNormalMap = detail::registerTextureFromRGBA(
                                         library,
                                         graphicsContext.device(),
                                         graphicsContext.allocator(),
                                         graphicsContext.universalQueue(),
                                         submissionQueue,
                                         VK_FORMAT_R8G8B8A8_UNORM,
                                         "defaultNormal",
                                         defaultImage,
                                         std::nullopt
        )
                                         .value();
    }

    { // Plane mesh
        std::vector<VertexPacked> vertices{
            VertexPacked{
                .position = glm::vec3{-1.0F, 0.0F, 1.0F},
                .uv_x = 0.0F,
                .normal = glm::vec3{0.0F, -1.0F, 0.0F},
                .uv_y = 0.0F,
                .color = glm::vec4{1.0F}
            },
            VertexPacked{
                .position = glm::vec3{1.0F, 0.0F, 1.0F},
                .uv_x = 1.0F,
                .normal = glm::vec3{0.0F, -1.0F, 0.0F},
                .uv_y = 0.0F,
                .color = glm::vec4{1.0F}
            },
            VertexPacked{
                .position = glm::vec3{1.0F, 0.0F, -1.0F},
                .uv_x = 1.0F,
                .normal = glm::vec3{0.0F, -1.0F, 0.0F},
                .uv_y = 1.0F,
                .color = glm::vec4{1.0F}
            },
            VertexPacked{
                .position = glm::vec3{-1.0F, 0.0F, -1.0F},
                .uv_x = 0.0F,
                .normal = glm::vec3{0.0F, -1.0F, 0.0F},
                .uv_y = 1.0F,
                .color = glm::vec4{1.0F}
            },
        };

        std::vector<uint32_t> indices{0, 1, 3, 1, 2, 3};

        std::vector<GeometrySurface> surfaces{GeometrySurface{
            .firstIndex = 0,
            .indexCount = 6,
            .material =
                MaterialData{
                    .ORM = library.m_defaultORMMap,
                    .normal = library.m_defaultNormalMap,
                    .color = library.m_defaultColorMap,
                }
        }};

        glm::vec3 vertexMinimum{std::numeric_limits<float>::max()};
        glm::vec3 vertexMaximum{std::numeric_limits<float>::lowest()};

        for (syzygy::VertexPacked const& vertex : vertices)
        {
            vertexMinimum = glm::min(vertex.position, vertexMinimum);
            vertexMaximum = glm::max(vertex.position, vertexMaximum);
        }

        auto newMesh = std::make_unique<syzygy::Mesh>(syzygy::Mesh{
            .surfaces = std::move(surfaces),
            .vertexBounds = syzygy::AABB::create(vertexMinimum, vertexMaximum),
            .meshBuffers = detail::uploadMeshToGPU(
                graphicsContext.device(),
                graphicsContext.allocator(),
                graphicsContext.universalQueue(),
                submissionQueue,
                indices,
                vertices
            ),
        });

        library.m_meshPlane =
            library
                .registerAsset<Mesh>(
                    std::move(newMesh), "mesh_Plane", std::nullopt
                )
                .value();
    }

    { // Cube mesh, all faces have same tex coords
        auto const addCubeFace{[](std::vector<VertexPacked>& vertices,
                                  std::vector<uint32_t>& indices,
                                  glm::vec3 const uvOrigin,
                                  glm::vec3 const uvX,
                                  glm::vec3 const uvY,
                                  glm::vec3 const normal)
        {
            uint32_t const startingIndex{static_cast<uint32_t>(vertices.size())
            };
            vertices.push_back(VertexPacked{
                .position = uvOrigin,
                .uv_x = 0.0F,
                .normal = normal,
                .uv_y = 0.0F,
            });
            vertices.push_back(VertexPacked{
                .position = uvOrigin + uvX,
                .uv_x = 1.0F,
                .normal = normal,
                .uv_y = 0.0F,
            });
            vertices.push_back(VertexPacked{
                .position = uvOrigin + uvX + uvY,
                .uv_x = 1.0F,
                .normal = normal,
                .uv_y = 1.0F,
            });
            vertices.push_back(VertexPacked{
                .position = uvOrigin + uvY,
                .uv_x = 0.0F,
                .normal = normal,
                .uv_y = 1.0F,
            });

            indices.push_back(startingIndex);
            indices.push_back(startingIndex + 1);
            indices.push_back(startingIndex + 2);
            indices.push_back(startingIndex);
            indices.push_back(startingIndex + 2);
            indices.push_back(startingIndex + 3);
        }};

        std::vector<VertexPacked> vertices{};
        vertices.reserve(6 * 4);
        std::vector<uint32_t> indices{};
        indices.reserve(6 * 6);

        addCubeFace(
            vertices,
            indices,
            glm::vec3{-1.0F, -1.0F, 1.0F},
            glm::vec3{2.0F, 0.0F, 0.0F},
            glm::vec3{0.0F, 0.0F, -2.0F},
            glm::vec3{0.0F, -1.0F, 0.0F}
        );
        addCubeFace(
            vertices,
            indices,
            glm::vec3{-1.0F, 1.0F, -1.0F},
            glm::vec3{2.0F, 0.0F, 0.0F},
            glm::vec3{0.0F, 0.0F, 2.0F},
            glm::vec3{0.0F, 1.0F, 0.0F}
        );
        addCubeFace(
            vertices,
            indices,
            glm::vec3{1.0F, -1.0F, -1.0F},
            glm::vec3{0.0F, 0.0F, 2.0F},
            glm::vec3{0.0F, 2.0F, 0.0F},
            glm::vec3{1.0F, 0.0F, 0.0F}
        );
        addCubeFace(
            vertices,
            indices,
            glm::vec3{-1.0F, -1.0F, 1.0F},
            glm::vec3{0.0F, 0.0F, -2.0F},
            glm::vec3{0.0F, 2.0F, 0.0F},
            glm::vec3{-1.0F, 0.0F, 0.0F}
        );
        addCubeFace(
            vertices,
            indices,
            glm::vec3{-1.0F, -1.0F, -1.0F},
            glm::vec3{2.0F, 0.0F, 0.0F},
            glm::vec3{0.0F, 2.0F, 0.0F},
            glm::vec3{0.0F, 0.0F, -1.0F}
        );
        addCubeFace(
            vertices,
            indices,
            glm::vec3{1.0F, -1.0F, 1.0F},
            glm::vec3{-2.0F, 0.0F, 0.0F},
            glm::vec3{0.0F, 2.0F, 0.0F},
            glm::vec3{0.0F, 0.0F, 1.0F}
        );

        std::vector<GeometrySurface> surfaces{GeometrySurface{
            .firstIndex = 0,
            .indexCount = static_cast<uint32_t>(indices.size()),
            .material =
                MaterialData{
                    .ORM = library.m_defaultORMMap,
                    .normal = library.m_defaultNormalMap,
                    .color = library.m_defaultColorMap,
                }
        }};

        glm::vec3 vertexMinimum{std::numeric_limits<float>::max()};
        glm::vec3 vertexMaximum{std::numeric_limits<float>::lowest()};

        for (syzygy::VertexPacked const& vertex : vertices)
        {
            vertexMinimum = glm::min(vertex.position, vertexMinimum);
            vertexMaximum = glm::max(vertex.position, vertexMaximum);
        }

        auto newMesh = std::make_unique<syzygy::Mesh>(syzygy::Mesh{
            .surfaces = std::move(surfaces),
            .vertexBounds = syzygy::AABB::create(vertexMinimum, vertexMaximum),
            .meshBuffers = detail::uploadMeshToGPU(
                graphicsContext.device(),
                graphicsContext.allocator(),
                graphicsContext.universalQueue(),
                submissionQueue,
                indices,
                vertices
            ),
        });

        library.m_meshCube =
            library
                .registerAsset<Mesh>(
                    std::move(newMesh), "mesh_Cube", std::nullopt
                )
                .value();
    }

    return libraryResult;
}
void AssetLibrary::processTasks(
    GraphicsContext& graphicsContext,
    ImmediateSubmissionQueue const& submissionQueue
)
{
    for (std::shared_ptr<ImageLoadingTask> const& task : m_tasks)
    {
        if (task->status != TaskStatus::Success)
        {
            continue;
        }

        size_t loaded{0};
        for (ImageDiskSource const& source : task->loadees)
        {
            VkFormat const fileFormat{
                source.nonlinearEncoding ? VK_FORMAT_R8G8B8A8_SRGB
                                         : VK_FORMAT_R8G8B8A8_UNORM
            };

            // TODO: more usage flags necessary here, such as sampled
            if (loadTextureFromPath(
                    graphicsContext.device(),
                    graphicsContext.allocator(),
                    graphicsContext.universalQueue(),
                    submissionQueue,
                    fileFormat,
                    source.path
                )
                    .has_value())
            {
                loaded++;
            }
        }

        SZG_INFO("Finished Task: Loaded {} textures.", loaded);
    }

    size_t const taskCount{m_tasks.size()};
    m_tasks.erase(
        std::remove_if(
            m_tasks.begin(),
            m_tasks.end(),
            [](std::shared_ptr<ImageLoadingTask> const& task)
    { return task == nullptr || task->status != TaskStatus::Waiting; }
        ),
        m_tasks.end()
    );
    if (m_tasks.size() < taskCount)
    {
        SZG_INFO("AssetLibrary: Culled {} tasks.", taskCount - m_tasks.size());
    }
}
auto AssetLibrary::defaultMesh(DefaultMeshAssets const asset) -> AssetPtr<Mesh>
{
    switch (asset)
    {
    case DefaultMeshAssets::Cube:
        return m_meshCube;
    case DefaultMeshAssets::Plane:
        return m_meshPlane;
    }
}
auto AssetLibrary::deduplicateAssetName(std::string const& name) -> std::string
{
    size_t& nameCount{m_nameDuplicationCounters[name]};

    nameCount += 1;

    size_t const newNameSuffix{nameCount};

    if (newNameSuffix == 1ULL)
    {
        return name;
    }

    return fmt::format("{}_{}", name, nameCount);
}
} // namespace syzygy