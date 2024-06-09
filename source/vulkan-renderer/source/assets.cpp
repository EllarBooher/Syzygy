#include "assets.hpp"

#include "engine.hpp"
#include "initializers.hpp"

#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <fstream>

#include "helpers.hpp"

auto loadGltfMeshes(Engine *const engine, std::string const &localPath)
    -> std::optional<std::vector<std::shared_ptr<MeshAsset>>>
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
    if (!load) {
        Error(fmt::format(
            "Failed to load glTF: {}"
            , fastgltf::to_underlying(load.error()))
        );
        return {};
    }
    fastgltf::Asset const gltf{ std::move(load.get()) };

    std::vector<std::shared_ptr<MeshAsset>> newMeshes{};
    for (fastgltf::Mesh const& mesh : gltf.meshes) {
        std::vector<uint32_t> indices{};
        std::vector<Vertex> vertices{};

        std::vector<GeometrySurface> surfaces{};

        // Proliferate indices and vertices
        for (auto&& primitive : mesh.primitives)
        {
            surfaces.push_back(GeometrySurface{
                .firstIndex = static_cast<uint32_t>(indices.size()),
                .indexCount = static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count),
            });

            size_t const initialVertexIndex{ vertices.size() };

            { // Indices, not optional
                fastgltf::Accessor const& indexAccessor{ 
                    gltf.accessors[primitive.indicesAccessor.value()] 
                };
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                    [&](std::uint32_t index) {
                        indices.push_back(index + initialVertexIndex);
                    }
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
                    [&](glm::vec3 position, size_t index)
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
                auto const *const normals{primitive.findAttribute("NORMAL")};
                if (normals != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        gltf
                        , gltf.accessors[(*normals).second]
                        , [&](glm::vec3 normal, size_t index) 
                        {
                            vertices[initialVertexIndex + index].normal = normal;
                        }
                    );
                }
            }

            { // UVs
                auto const *const uvs{primitive.findAttribute("TEXCOORD_0")};
                if (uvs != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        gltf
                        , gltf.accessors[(*uvs).second]
                        , [&](glm::vec2 texcoord, size_t index) {
                            vertices[initialVertexIndex + index].uv_x = texcoord.x;
                            vertices[initialVertexIndex + index].uv_y = texcoord.y;
                        }
                    );
                }
            }

            { // Colors
                auto const *const colors{primitive.findAttribute("COLOR_0")};
                if (colors != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        gltf
                        , gltf.accessors[(*colors).second]
                        , [&](glm::vec4 color, size_t index) {
                            vertices[initialVertexIndex + index].color = color;
                        }
                    );
                }
            }
        }

        bool constexpr DEBUG_OVERRIDE_COLORS{ false };
        if (DEBUG_OVERRIDE_COLORS) {
            for (Vertex& vertex : vertices) {
                vertex.color = glm::vec4(vertex.normal, 1.0F);
            }
        }

        bool constexpr FLIP_Y{ true };
        if (FLIP_Y) {
            for (Vertex& vertex : vertices) {
                vertex.normal.y *= -1;
                vertex.position.y *= -1;
            }
        }

        newMeshes.push_back(
            std::make_shared<MeshAsset>(
                MeshAsset{
                    .name = std::string{ mesh.name },
                    .surfaces = surfaces,
                    .meshBuffers = engine->uploadMeshToGPU(indices, vertices),
                }
            )
        );
    }

    return newMeshes;
}

auto loadAssetFile(std::string const &localPath, VkDevice const device) -> AssetLoadingResult
{
    std::unique_ptr<std::filesystem::path> const pPath{
        DebugUtils::getLoadedDebugUtils().loadAssetPath(std::filesystem::path(localPath))
    };
    if (pPath == nullptr)
    {
        return AssetLoadingError{
            .message =
                fmt::format(
                    "Unable to parse path at \"{}\", this indicates the asset "
                    "does not exist or the path is malformed"
                    , localPath
                ),
        };
    }
    std::filesystem::path const path = *pPath;

    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return AssetLoadingError{
            .message =
                fmt::format(
                    "Unable to parse path at \"{}\", this indicates the asset "
                    "does not exist or the path is malformed"
                    , localPath
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
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSizeBytes));

    file.close();

    return AssetFile{
        .fileName = path.filename().string(),
        .fileBytes = buffer,
    };
}
