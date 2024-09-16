#include "assets.hpp"

#include "syzygy/core/immediate.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/editor/graphicscontext.hpp"
#include "syzygy/platform/filesystemutils.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/platformutils.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageview.hpp"
#include <algorithm>
#include <cassert>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp> // IWYU pragma: keep
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <limits>
#include <span>
#include <spdlog/fmt/bundled/core.h>
#include <utility>
#include <variant>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace
{
struct ImageRGBA
{
    uint32_t x{0};
    uint32_t y{0};
    std::vector<uint8_t> bytes{};
};

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

} // namespace

namespace detail_stbi
{
auto RGBAfromJPEG(std::span<uint8_t const> const jpegBytes)
    -> std::optional<ImageRGBA>
{
    int32_t x{0};
    int32_t y{0};

    int32_t components{0};
    uint16_t constexpr RGBA_COMPONENT_COUNT{4};

    stbi_uc* const parsedImage{stbi_load_from_memory(
        jpegBytes.data(),
        static_cast<int32_t>(jpegBytes.size()),
        &x,
        &y,
        &components,
        RGBA_COMPONENT_COUNT
    )};

    if (parsedImage == nullptr)
    {
        SZG_ERROR("Parsed image is null.");
        return std::nullopt;
    }

    if (x < 1 || y < 1)
    {
        SZG_ERROR(
            fmt::format("Parsed JPEG had invalid dimensions: ({},{})", x, y)
        );
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

    delete parsedImage;

    return ImageRGBA{.x = widthPixels, .y = heightPixels, .bytes = rgba};
}

} // namespace detail_stbi

namespace detail_fastgltf
{
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

struct LoadedImage
{
    ImageRGBA data{};
    std::filesystem::path source{};
};

// Preserves gltf indexing
auto loadRGBA(
    std::span<fastgltf::Image const> const gltfImages,
    std::filesystem::path const& assetRoot
) -> std::vector<std::optional<LoadedImage>>
{
    std::vector<std::optional<LoadedImage>> rawImagesByGLTFIndex{};
    rawImagesByGLTFIndex.reserve(gltfImages.size());
    for (fastgltf::Image const& image : gltfImages)
    {
        rawImagesByGLTFIndex.push_back(std::nullopt);
        LoadedImage loadedImage{};

        std::optional<ImageRGBA> imageConvertResult{std::nullopt};
        if (std::holds_alternative<fastgltf::sources::Array>(image.data))
        {
            std::span<uint8_t const> const data =
                std::get<fastgltf::sources::Array>(image.data).bytes;

            imageConvertResult = detail_stbi::RGBAfromJPEG(data);
        }
        else if (std::holds_alternative<fastgltf::sources::URI>(image.data))
        {
            fastgltf::sources::URI const& uri{
                std::get<fastgltf::sources::URI>(image.data)
            };

            assert(uri.fileByteOffset == 0);
            assert(uri.mimeType == fastgltf::MimeType::JPEG);
            assert(uri.uri.isLocalPath());

            std::filesystem::path const path{assetRoot / uri.uri.fspath()};

            std::ifstream file(path, std::ios::binary);

            assert(std::filesystem::exists(path));
            size_t const lengthBytes{std::filesystem::file_size(path)};

            std::vector<uint8_t> data(lengthBytes);
            file.read(reinterpret_cast<char*>(data.data()), lengthBytes);

            imageConvertResult = detail_stbi::RGBAfromJPEG(data);

            loadedImage.source = path;
        }
        else
        {
            SZG_WARNING("Unsupported glTF image source found.");
            continue;
        }

        if (!imageConvertResult.has_value())
        {
            SZG_WARNING("Failed to convert glTF image from JPEG.");
            continue;
        }

        loadedImage.data = imageConvertResult.value();

        rawImagesByGLTFIndex.back() = std::move(loadedImage);
    }

    return rawImagesByGLTFIndex;
}

// Preserves glTF indexing
auto loadTextures(
    std::span<fastgltf::Texture const> const gltfTextures,
    std::span<std::optional<LoadedImage> const> const imagesByGLTFIndex
) -> std::vector<std::optional<std::reference_wrapper<LoadedImage const>>>
{
    std::vector<std::optional<std::reference_wrapper<LoadedImage const>>>
        texturesByGLTFIndex{};
    texturesByGLTFIndex.reserve(gltfTextures.size());
    for (fastgltf::Texture const& texture : gltfTextures)
    {
        texturesByGLTFIndex.push_back(std::nullopt);
        auto& currentTexture{texturesByGLTFIndex.back()};

        if (!texture.imageIndex.has_value())
        {
            SZG_WARNING("Texture {} was missing imageIndex.", texture.name);
            continue;
        }

        size_t const loadedIndex{texture.imageIndex.value()};

        if (loadedIndex >= gltfTextures.size())
        {
            SZG_WARNING(
                "Texture {} had imageIndex that was out of bounds.",
                texture.name
            );
            continue;
        }

        if (!imagesByGLTFIndex[loadedIndex].has_value())
        {
            SZG_WARNING(
                "Texture {} referred to image that could not be loaded.",
                texture.name
            );
            continue;
        }

        currentTexture = imagesByGLTFIndex[loadedIndex].value();
    }

    return texturesByGLTFIndex;
}

auto uploadTexture(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const universalQueue,
    syzygy::ImmediateSubmissionQueue const& submissionQueue,
    std::span<
        std::optional<std::reference_wrapper<LoadedImage const>> const> const
        texturesByGLTFIndex,
    VkFormat const textureFormat,
    size_t const textureIndex
) -> std::optional<std::unique_ptr<syzygy::ImageView>>
{
    if (textureIndex >= texturesByGLTFIndex.size())
    {
        SZG_ERROR(
            "Out of bounds texture index {}, have {} textures.",
            textureIndex,
            texturesByGLTFIndex.size()
        );
        return std::nullopt;
    }

    std::optional<std::reference_wrapper<LoadedImage const>> const textureRef{
        texturesByGLTFIndex[textureIndex]
    };
    if (!textureRef.has_value())
    {
        SZG_ERROR("Texture at gltf index {} was not loaded.", textureIndex);
        return std::nullopt;
    }

    std::optional<std::unique_ptr<syzygy::Image>> imageUploadResult{
        uploadImageToGPU(
            device,
            allocator,
            universalQueue,
            submissionQueue,
            textureFormat,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            textureRef.value().get().data
        )
    };

    if (!imageUploadResult.has_value() || imageUploadResult.value() == nullptr)
    {
        SZG_ERROR("Failed to upload glTF image to GPU.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<syzygy::ImageView>> imageViewResult{
        syzygy::ImageView::allocate(
            device,
            allocator,
            std::move(*imageUploadResult.value()),
            syzygy::ImageViewAllocationParameters{}
        )
    };
    if (!imageViewResult.has_value() || imageViewResult.value() == nullptr)
    {
        SZG_ERROR("Failed to convert image into imageview.");
        return std::nullopt;
    }

    return std::move(imageViewResult).value();
}

struct MaterialTextureIndices
{
    std::optional<size_t> color{};
    std::optional<size_t> normal{};
    std::optional<size_t> ORM{};
};

// Preserves gltf indexing. Returns a vector whose size matches the count of
// materials passed in.
auto collectMaterialTextureIndices(
    std::span<fastgltf::Material const> const materials
) -> std::vector<MaterialTextureIndices>
{
    std::vector<MaterialTextureIndices> textures{};
    textures.reserve(materials.size());
    for (fastgltf::Material const& material : materials)
    {
        MaterialTextureIndices texture{};

        {
            std::optional<fastgltf::TextureInfo> const& metallicRoughness{
                material.pbrData.metallicRoughnessTexture
            };
            std::optional<fastgltf::OcclusionTextureInfo> const& occlusion{
                material.occlusionTexture
            };
            if (!metallicRoughness.has_value() && !occlusion.has_value())
            {
                SZG_WARNING(
                    "Material {}: Missing MetallicRoughness and Occlusion "
                    "textures.",
                    material.name
                );
            }
            else
            {
                if ((!metallicRoughness.has_value() || !occlusion.has_value())
                    || metallicRoughness.value().textureIndex
                           != occlusion.value().textureIndex)
                {
                    SZG_WARNING(
                        "Material {}: Occlusion and MetallicRoughness differ. "
                        "Loading {} and using its textures' RGB channels for "
                        "the "
                        "ORM map.",
                        material.name,
                        metallicRoughness.has_value() ? "MetallicRoughness"
                                                      : "Occlusion"
                    );
                }

                texture.ORM = metallicRoughness.has_value()
                                ? metallicRoughness.value().textureIndex
                                : occlusion.value().textureIndex;
            }
        }

        {
            std::optional<fastgltf::TextureInfo> const& color{
                material.pbrData.baseColorTexture
            };
            if (!color.has_value())
            {
                SZG_WARNING(
                    "Material {}: Missing color texture.", material.name
                );
            }
            else
            {
                texture.color = color.value().textureIndex;
            }
        }

        {
            std::optional<fastgltf::NormalTextureInfo> const& normal{
                material.normalTexture
            };
            if (!normal.has_value())
            {
                SZG_WARNING(
                    "Material {}: Missing normal texture.", material.name
                );
            }
            else
            {
                texture.normal = normal.value().textureIndex;
            }
        }

        textures.push_back(texture);
    }

    assert(textures.size() == materials.size());

    return textures;
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
            .meshBuffers = uploadMeshToGPU(
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
    std::filesystem::path const& filePath,
    VkImageUsageFlags const additionalFlags
) -> std::optional<Asset<ImageView>>
{
    SZG_INFO("Loading Texture from '{}'", filePath.string());
    std::optional<AssetFile> const fileResult{loadAssetFile(filePath)};
    if (!fileResult.has_value())
    {
        SZG_ERROR("Failed to load file for texture at '{}'", filePath.string());
        return std::nullopt;
    }

    AssetFile const& file{fileResult.value()};

    if (std::filesystem::path const extension{file.path.extension()};
        extension != ".jpg" && extension != ".jpeg")
    {
        SZG_ERROR("Only JPEGs are supported for loading textures.");
        return std::nullopt;
    }

    std::optional<ImageRGBA> imageResult{
        detail_stbi::RGBAfromJPEG(file.fileBytes)
    };
    if (!imageResult.has_value())
    {
        SZG_ERROR("Failed to convert file from JPEG.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<syzygy::Image>> uploadResult{uploadImageToGPU(
        device,
        allocator,
        transferQueue,
        submissionQueue,
        VK_FORMAT_R8G8B8A8_SRGB,
        additionalFlags,
        imageResult.value()
    )};
    if (!uploadResult.has_value())
    {
        SZG_ERROR("Failed to upload image to GPU.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<ImageView>> textureResult{ImageView::allocate(
        device,
        allocator,
        std::move(*uploadResult.value()),
        ImageViewAllocationParameters{}
    )};

    return std::optional<Asset<ImageView>>{Asset<ImageView>{
        .metadata =
            syzygy::AssetMetadata{
                .displayName = file.path.filename().string(),
                .fileLocalPath = file.path.string(),
                .id = syzygy::UUID::createNew(),
            },
        .data = std::move(textureResult).value(),
    }};
}

void AssetLibrary::loadTexturesDialog(
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

    size_t loaded{0};
    for (auto const& path : paths)
    {
        auto textureLoadResult{loadTextureFromPath(
            graphicsContext.device(),
            graphicsContext.allocator(),
            graphicsContext.universalQueue(),
            submissionQueue,
            path,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        )};
        if (!textureLoadResult.has_value())
        {
            continue;
        }

        m_textures.push_back(std::move(textureLoadResult).value());
        loaded++;
    }

    if (loaded > 0)
    {
        SZG_INFO("Loaded {} textures.", loaded);
    }
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

    // We load the images as raw bytes, then defer decoding/upload to GPU until
    // we know how each image is used. E.g., albedo is nonlinear encoded and
    // normal maps are linearly encoded.
    std::vector<std::optional<detail_fastgltf::LoadedImage>> const
        imagesByGLTFIndex{
            detail_fastgltf::loadRGBA(gltf.images, filePath.parent_path())
        };

    // Don't load samplers, just load the image data.
    std::vector<std::optional<
        std::reference_wrapper<detail_fastgltf::LoadedImage const>>> const
        texturesByGLTFIndex{
            detail_fastgltf::loadTextures(gltf.textures, imagesByGLTFIndex)
        };

    MaterialData const defaultMaterialData{
        .ORM = m_textures[m_defaultORMIndex].data,
        .normal = m_textures[m_defaultNormalIndex].data,
        .color = m_textures[m_defaultColorIndex].data,
    };

    std::vector<detail_fastgltf::MaterialTextureIndices> const
        materialIndicesByGLTFIndex{
            detail_fastgltf::collectMaterialTextureIndices(gltf.materials)
        };

    std::vector<MaterialData> materialDataByGLTFIndex{};
    materialDataByGLTFIndex.reserve(materialIndicesByGLTFIndex.size());
    for (size_t materialIndex{0};
         materialIndex < materialIndicesByGLTFIndex.size();
         materialIndex++)
    {
        materialDataByGLTFIndex.push_back(defaultMaterialData);
        MaterialData& materialData{materialDataByGLTFIndex.back()};
        fastgltf::Material const& material{gltf.materials[materialIndex]};

        detail_fastgltf::MaterialTextureIndices materialTextures{
            materialIndicesByGLTFIndex[materialIndex]
        };

        if (materialTextures.ORM.has_value())
        {
            size_t const ORMIndex{materialTextures.ORM.value()};

            if (std::optional<std::unique_ptr<syzygy::ImageView>>
                    ormTextureUploadResult{detail_fastgltf::uploadTexture(
                        graphicsContext.device(),
                        graphicsContext.allocator(),
                        graphicsContext.universalQueue(),
                        submissionQueue,
                        texturesByGLTFIndex,
                        VK_FORMAT_R8G8B8A8_UNORM,
                        ORMIndex
                    )};
                !ormTextureUploadResult.has_value()
                || ormTextureUploadResult.value() == nullptr)
            {
                SZG_WARNING(
                    "Material {}: Failed to upload ORM texture.", material.name
                );
            }
            else
            {
                std::shared_ptr<syzygy::ImageView> ormPointer{
                    std::move(ormTextureUploadResult).value()
                };
                std::filesystem::path textureSourcePath{
                    texturesByGLTFIndex[ORMIndex].value().get().source
                };

                m_textures.push_back(Asset<ImageView>{
                    .metadata =
                        AssetMetadata{
                            .displayName = deduplicateAssetName(
                                textureSourcePath.stem().string()
                            ),
                            .fileLocalPath = textureSourcePath.string(),
                            .id = UUID::createNew()
                        },
                    .data = ormPointer
                });

                materialData.ORM = ormPointer;
            }
        }

        if (materialTextures.color.has_value())
        {
            size_t const colorIndex{materialTextures.color.value()};

            if (std::optional<std::unique_ptr<syzygy::ImageView>>
                    colorTextureUploadResult{uploadTexture(
                        graphicsContext.device(),
                        graphicsContext.allocator(),
                        graphicsContext.universalQueue(),
                        submissionQueue,
                        texturesByGLTFIndex,
                        VK_FORMAT_R8G8B8A8_SRGB,
                        colorIndex
                    )};
                !colorTextureUploadResult.has_value()
                || colorTextureUploadResult.value() == nullptr)
            {
                SZG_WARNING(
                    "Material {}: Failed to upload color texture.",
                    material.name
                );
            }
            else
            {
                std::shared_ptr<syzygy::ImageView> colorPointer{
                    std::move(colorTextureUploadResult).value()
                };
                std::filesystem::path textureSourcePath{
                    texturesByGLTFIndex[colorIndex].value().get().source
                };

                m_textures.push_back(Asset<ImageView>{
                    .metadata =
                        AssetMetadata{
                            .displayName = deduplicateAssetName(
                                textureSourcePath.stem().string()
                            ),
                            .fileLocalPath = textureSourcePath.string(),
                            .id = UUID::createNew()
                        },
                    .data = colorPointer
                });

                materialData.color = colorPointer;
            }
        }

        if (materialTextures.normal.has_value())
        {
            size_t const normalIndex{materialTextures.normal.value()};

            if (std::optional<std::unique_ptr<syzygy::ImageView>>
                    normalTextureUploadResult{uploadTexture(
                        graphicsContext.device(),
                        graphicsContext.allocator(),
                        graphicsContext.universalQueue(),
                        submissionQueue,
                        texturesByGLTFIndex,
                        VK_FORMAT_R8G8B8A8_UNORM,
                        normalIndex
                    )};
                !normalTextureUploadResult.has_value()
                || normalTextureUploadResult.value() == nullptr)
            {
                SZG_WARNING(
                    "Material {}: Failed to upload normal texture.",
                    material.name
                );
            }
            else
            {

                std::shared_ptr<syzygy::ImageView> normalPointer{
                    std::move(normalTextureUploadResult).value()
                };
                std::filesystem::path textureSourcePath{
                    texturesByGLTFIndex[normalIndex].value().get().source
                };

                m_textures.push_back(Asset<ImageView>{
                    .metadata =
                        AssetMetadata{
                            .displayName = deduplicateAssetName(
                                textureSourcePath.stem().string()
                            ),
                            .fileLocalPath = textureSourcePath.string(),
                            .id = UUID::createNew()
                        },
                    .data = normalPointer
                });

                materialData.normal = normalPointer;
            }
        }
    }

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
    SZG_INFO("Loaded {} meshes from glTF", newMeshes.size());

    for (size_t gltfMeshIndex{0}; gltfMeshIndex < newMeshes.size();
         gltfMeshIndex++)
    {
        std::unique_ptr<Mesh>& pMesh{newMeshes[gltfMeshIndex]};

        if (pMesh == nullptr)
        {
            continue;
        }

        m_meshes.push_back(Asset<Mesh>{
            .metadata =
                AssetMetadata{
                    .displayName = deduplicateAssetName(
                        std::format("mesh_{}", gltf.meshes[gltfMeshIndex].name)
                    ),
                    .fileLocalPath = filePath.string(),
                    .id = UUID::createNew()
                },
            .data = std::move(pMesh),
        });
    }
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

    struct RGBATexel
    {
        uint8_t r{0};
        uint8_t g{0};
        uint8_t b{0};
        uint8_t a{std::numeric_limits<uint8_t>::max()};
    };

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

        std::optional<std::unique_ptr<syzygy::Image>> uploadResult{
            uploadImageToGPU(
                graphicsContext.device(),
                graphicsContext.allocator(),
                graphicsContext.universalQueue(),
                submissionQueue,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                defaultImage
            )
        };
        if (!uploadResult.has_value())
        {
            SZG_ERROR("Failed to upload default color texture to GPU.");
            return std::nullopt;
        }
        std::optional<std::unique_ptr<syzygy::ImageView>> imageViewResult{
            syzygy::ImageView::allocate(
                graphicsContext.device(),
                graphicsContext.allocator(),
                std::move(*uploadResult.value()),
                syzygy::ImageViewAllocationParameters{}
            )
        };
        if (!imageViewResult.has_value() || imageViewResult.value() == nullptr)
        {
            SZG_ERROR(
                "Failed to convert default color texture image into imageview."
            );
            return std::nullopt;
        }

        library.m_defaultColorIndex = library.m_textures.size();
        library.m_textures.push_back(syzygy::Asset<syzygy::ImageView>{
            .metadata =
                syzygy::AssetMetadata{
                    .displayName =
                        library.deduplicateAssetName("texture_DefaultColor"),
                    .fileLocalPath = "",
                    .id = syzygy::UUID::createNew(),
                },
            .data = std::move(imageViewResult).value(),
        });
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

        std::optional<std::unique_ptr<syzygy::Image>> uploadResult{
            uploadImageToGPU(
                graphicsContext.device(),
                graphicsContext.allocator(),
                graphicsContext.universalQueue(),
                submissionQueue,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                defaultImage
            )
        };
        if (!uploadResult.has_value())
        {
            SZG_ERROR("Failed to upload default normal texture to GPU.");
            return std::nullopt;
        }
        std::optional<std::unique_ptr<syzygy::ImageView>> imageViewResult{
            syzygy::ImageView::allocate(
                graphicsContext.device(),
                graphicsContext.allocator(),
                std::move(*uploadResult.value()),
                syzygy::ImageViewAllocationParameters{}
            )
        };
        if (!imageViewResult.has_value() || imageViewResult.value() == nullptr)
        {
            SZG_ERROR(
                "Failed to convert default normal texture image into imageview."
            );
            return std::nullopt;
        }

        library.m_defaultNormalIndex = library.m_textures.size();
        library.m_textures.push_back(syzygy::Asset<syzygy::ImageView>{
            .metadata =
                syzygy::AssetMetadata{
                    .displayName =
                        library.deduplicateAssetName("texture_DefaultNormal"),
                    .fileLocalPath = "",
                    .id = syzygy::UUID::createNew(),
                },
            .data = std::move(imageViewResult).value(),
        });
    }
    {
        // Default ORM texture

        size_t index{0};
        for (RGBATexel& texel : std::span<RGBATexel>{
                 reinterpret_cast<RGBATexel*>(defaultImage.bytes.data()),
                 defaultImage.bytes.size() / sizeof(RGBATexel)
             })
        {
            size_t const x{index % defaultImage.x};
            size_t const y{index / defaultImage.x};

            RGBATexel constexpr NONMETALLIC_SQUARE{
                .r = 255U, .g = 30U, .b = 0U, .a = 0U
            };
            RGBATexel constexpr METALLIC_SQUARE{
                .r = 255U, .g = 30U, .b = 255U, .a = 0U
            };

            bool const nonmetallic{((x / 8) + (y / 8)) % 2 == 0};

            texel = nonmetallic ? NONMETALLIC_SQUARE : METALLIC_SQUARE;

            index++;
        }

        std::optional<std::unique_ptr<syzygy::Image>> uploadResult{
            uploadImageToGPU(
                graphicsContext.device(),
                graphicsContext.allocator(),
                graphicsContext.universalQueue(),
                submissionQueue,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                defaultImage
            )
        };
        if (!uploadResult.has_value())
        {
            SZG_ERROR("Failed to upload default ORM texture to GPU.");
            return std::nullopt;
        }
        std::optional<std::unique_ptr<syzygy::ImageView>> imageViewResult{
            syzygy::ImageView::allocate(
                graphicsContext.device(),
                graphicsContext.allocator(),
                std::move(*uploadResult.value()),
                syzygy::ImageViewAllocationParameters{}
            )
        };
        if (!imageViewResult.has_value() || imageViewResult.value() == nullptr)
        {
            SZG_ERROR(
                "Failed to convert default ORM texture image into imageview."
            );
            return std::nullopt;
        }

        library.m_defaultORMIndex = library.m_textures.size();
        library.m_textures.push_back(syzygy::Asset<syzygy::ImageView>{
            .metadata =
                syzygy::AssetMetadata{
                    .displayName =
                        library.deduplicateAssetName("texture_DefaultORM"),
                    .fileLocalPath = "",
                    .id = syzygy::UUID::createNew(),
                },
            .data = std::move(imageViewResult).value(),
        });
    }

    std::filesystem::path const assetsRoot{ensureAbsolutePath("assets")};
    if (!std::filesystem::exists(assetsRoot))
    {
        SZG_WARNING(
            "Default assets folder was NOT found in the working directory."
        );
    }
    else
    {
        SZG_INFO(
            "Default assets folder found, now attempting to load default scene."
        );

        std::filesystem::path const meshPath{
            assetsRoot / "vkguide\\basicmesh.glb"
        };
        library.loadGLTFFromPath(graphicsContext, submissionQueue, meshPath);
    }

    return libraryResult;
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