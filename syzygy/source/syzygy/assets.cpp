#include "assets.hpp"

#include "syzygy/buffers.hpp"
#include "syzygy/core/immediate.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/enginetypes.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/images/image.hpp"
#include <algorithm>
#include <cassert>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp> // IWYU pragma: keep
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/util.hpp>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <functional>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <span>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace
{
auto uploadMeshToGPU(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    ImmediateSubmissionQueue const& submissionQueue,
    std::span<uint32_t const> const indices,
    std::span<Vertex const> const vertices
) -> std::unique_ptr<GPUMeshBuffers>
{
    // Allocate buffer

    size_t const indexBufferSize{indices.size_bytes()};
    size_t const vertexBufferSize{vertices.size_bytes()};

    AllocatedBuffer indexBuffer{AllocatedBuffer::allocate(
        device,
        allocator,
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    )};

    AllocatedBuffer vertexBuffer{AllocatedBuffer::allocate(
        device,
        allocator,
        vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    )};

    // Copy data into buffer

    AllocatedBuffer stagingBuffer{AllocatedBuffer::allocate(
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
        result != ImmediateSubmissionQueue::SubmissionResult::SUCCESS)
    {
        Warning("Command submission for mesh upload failed, buffers will "
                "likely contain junk or no data.");
    }

    return std::make_unique<GPUMeshBuffers>(
        std::move(indexBuffer), std::move(vertexBuffer)
    );
}

auto RGBAfromJPEG_stbi(std::span<uint8_t const> const jpegBytes)
    -> std::optional<szg_assets::ImageRGBA>
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
        Error("Parsed image is null.");
        return std::nullopt;
    }

    if (x < 1 || y < 1)
    {
        Error(fmt::format("Parsed JPEG had invalid dimensions: ({},{})", x, y));
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

    return szg_assets::ImageRGBA{
        .x = widthPixels, .y = heightPixels, .bytes = rgba
    };
}

auto uploadImageToGPU(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    ImmediateSubmissionQueue const& submissionQueue,
    VkFormat const format,
    VkImageUsageFlags const additionalFlags,
    szg_assets::ImageRGBA const& image
) -> std::optional<std::unique_ptr<szg_image::Image>>
{
    VkExtent2D const imageExtent{.width = image.x, .height = image.y};

    std::optional<std::unique_ptr<szg_image::Image>> stagingImageResult{
        szg_image::Image::allocate(
            device,
            allocator,
            szg_image::ImageAllocationParameters{
                .extent = imageExtent,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                            | VK_IMAGE_USAGE_STORAGE_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
                .tiling = VK_IMAGE_TILING_LINEAR,
                .vmaUsage = VMA_MEMORY_USAGE_CPU_ONLY,
                .vmaFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
            }
        )
    };
    if (!stagingImageResult.has_value())
    {
        Error("Failed to allocate staging image.");
        return std::nullopt;
    }
    szg_image::Image& stagingImage{*stagingImageResult.value()};

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
        Error("Failed to map bytes of staging image.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<szg_image::Image>> finalImageResult{
        szg_image::Image::allocate(
            device,
            allocator,
            szg_image::ImageAllocationParameters{
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
        Error("Failed to allocate final image.");
        return std::nullopt;
    }
    szg_image::Image& finalImage{*finalImageResult.value()};

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

        szg_image::Image::recordCopyEntire(
            cmd, stagingImage, finalImage, VK_IMAGE_ASPECT_COLOR_BIT
        );
    }
        )};
        submissionResult != ImmediateSubmissionQueue::SubmissionResult::SUCCESS)
    {
        Error("Failed to copy images.");
        return std::nullopt;
    }

    return std::move(finalImageResult).value();
}
} // namespace

auto loadGltfMeshes(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    ImmediateSubmissionQueue const& submissionQueue,
    std::string const& localPath
) -> std::optional<std::vector<std::shared_ptr<MeshAsset>>>
{
    std::filesystem::path const assetPath{
        DebugUtils::getLoadedDebugUtils().makeAbsolutePath(localPath)
    };

    Log(fmt::format("Loading glTF: {}", assetPath.string()));

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(assetPath);

    auto constexpr GLTF_OPTIONS{
        fastgltf::Options::LoadGLBBuffers
        | fastgltf::Options::LoadExternalBuffers
    };

    fastgltf::Parser parser{};

    fastgltf::Expected<fastgltf::Asset> load{
        parser.loadGltfBinary(&data, assetPath.parent_path(), GLTF_OPTIONS)
    };
    if (!load)
    {
        Error(fmt::format(
            "Failed to load glTF: {}", fastgltf::to_underlying(load.error())
        ));
        return {};
    }
    fastgltf::Asset const gltf{std::move(load.get())};

    std::vector<std::shared_ptr<MeshAsset>> newMeshes{};
    for (fastgltf::Mesh const& mesh : gltf.meshes)
    {
        std::vector<uint32_t> indices{};
        std::vector<Vertex> vertices{};

        std::vector<GeometrySurface> surfaces{};

        // Proliferate indices and vertices
        for (auto&& primitive : mesh.primitives)
        {
            surfaces.push_back(GeometrySurface{
                .firstIndex = static_cast<uint32_t>(indices.size()),
                .indexCount = static_cast<uint32_t>(
                    gltf.accessors[primitive.indicesAccessor.value()].count
                ),
            });

            size_t const initialVertexIndex{vertices.size()};

            { // Indices, not optional
                fastgltf::Accessor const& indexAccessor{
                    gltf.accessors[primitive.indicesAccessor.value()]
                };
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<uint32_t>(
                    gltf,
                    indexAccessor,
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
                    vertices.push_back(Vertex{
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

        bool constexpr DEBUG_OVERRIDE_COLORS{false};
        if (DEBUG_OVERRIDE_COLORS)
        {
            for (Vertex& vertex : vertices)
            {
                vertex.color = glm::vec4(vertex.normal, 1.0F);
            }
        }

        bool constexpr FLIP_Y{true};
        if (FLIP_Y)
        {
            for (Vertex& vertex : vertices)
            {
                vertex.normal.y *= -1;
                vertex.position.y *= -1;
            }
        }

        newMeshes.push_back(std::make_shared<MeshAsset>(MeshAsset{
            .name = std::string{mesh.name},
            .surfaces = surfaces,
            .meshBuffers = uploadMeshToGPU(
                device,
                allocator,
                transferQueue,
                submissionQueue,
                indices,
                vertices
            ),
        }));
    }

    return newMeshes;
}

auto loadAssetFile(std::string const& localPath) -> AssetLoadingResult
{
    std::unique_ptr<std::filesystem::path> const pPath{
        DebugUtils::getLoadedDebugUtils().loadAssetPath(
            std::filesystem::path(localPath)
        )
    };
    if (pPath == nullptr)
    {
        return AssetLoadingError{
            .message = fmt::format(
                "Unable to parse path at \"{}\", this indicates the asset does "
                "not exist or the path is malformed",
                localPath
            ),
        };
    }
    std::filesystem::path const path = *pPath;

    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return AssetLoadingError{
            .message = fmt::format(
                "Unable to parse path at \"{}\", this indicates the asset does "
                "not exist or the path is malformed",
                localPath
            ),
        };
    }

    size_t const fileSizeBytes = static_cast<size_t>(file.tellg());
    if (fileSizeBytes == 0)
    {
        return AssetLoadingError{
            .message = fmt::format("Shader file is empty at \"{}\"", localPath),
        };
    }

    std::vector<uint8_t> buffer(fileSizeBytes);

    file.seekg(0, std::ios::beg);
    file.read(
        reinterpret_cast<char*>(buffer.data()),
        static_cast<std::streamsize>(fileSizeBytes)
    );

    file.close();

    return AssetFile{
        .fileName = path.filename().string(),
        .fileBytes = buffer,
    };
}

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
auto szg_assets::loadTextureFromFile(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    ImmediateSubmissionQueue const& submissionQueue,
    std::string const& localPath,
    VkImageUsageFlags const additionalFlags
) -> std::optional<Asset<szg_image::Image>>
{
    Log(fmt::format("Loading Texture from '{}'", localPath));
    AssetLoadingResult const fileResult{loadAssetFile(localPath)};
    return std::visit(
        overloaded{
            [&](AssetFile const& file)
    {
        std::optional<szg_assets::ImageRGBA> imageResult{
            RGBAfromJPEG_stbi(file.fileBytes)
        };
        if (!imageResult.has_value())
        {
            Error("Failed to convert file from JPEG.");
            return std::optional<Asset<szg_image::Image>>{};
        }

        std::optional<std::unique_ptr<szg_image::Image>> uploadResult{
            uploadImageToGPU(
                device,
                allocator,
                transferQueue,
                submissionQueue,
                VK_FORMAT_R8G8B8A8_UNORM,
                additionalFlags,
                imageResult.value()
            )
        };
        if (!uploadResult.has_value())
        {
            Error("Failed to upload image to GPU.");
            return std::optional<Asset<szg_image::Image>>{};
        }

        return std::optional<Asset<szg_image::Image>>{Asset<szg_image::Image>{
            .metadata =
                AssetMetadata{
                    .displayName = file.fileName,
                    .fileLocalPath = localPath,
                    .id = szg::UUID::createNew(),
                },
            .data = std::move(uploadResult).value(),
        }};
    },
            [&](AssetLoadingError const& error)
    {
        Error(fmt::format("Failed to load asset for texture: {}", error.message)
        );
        return std::optional<Asset<szg_image::Image>>{};
    }
        },
        fileResult
    );
}

void szg_assets::AssetLibrary::registerAsset(Asset<szg_image::Image>&& asset)
{
    m_textures.push_back(std::move(asset));
}

auto szg_assets::AssetLibrary::fetchAssets()
    -> std::vector<AssetRef<szg_image::Image>>
{
    std::vector<AssetRef<szg_image::Image>> assets{};

    assets.reserve(m_textures.size());
    for (auto& texture : m_textures)
    {
        assets.emplace_back(texture);
    }

    return assets;
}
