#include "scenemesh.hpp"

#include "syzygy/core/log.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include <spdlog/fmt/bundled/core.h>
#include <utility>

namespace syzygy
{
void MeshInstanced::setMesh(AssetPtr<Mesh> meshAsset)
{
    if (m_renderResources == nullptr)
    {
        m_renderResources = std::make_unique<MeshRenderResources>();
    }

    m_renderResources->mesh = std::move(meshAsset);
    m_surfaceDescriptorsDirty = true;
}

auto MeshInstanced::prepareForRendering(
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator,
    glm::mat4x4 const& worldMatrix
) -> std::optional<std::reference_wrapper<MeshRenderResources>>
{
    MeshRenderResources& resources{*m_renderResources};
    resources.castsShadow = castsShadow;

    std::shared_ptr<Asset<Mesh> const> const meshAsset{resources.mesh.lock()};
    if (meshAsset == nullptr || meshAsset->data == nullptr)
    {
        return std::nullopt;
    }
    Mesh const& mesh{*meshAsset->data};

    if (resources.models == nullptr
        || resources.modelInverseTransposes == nullptr
        || resources.models->stagingCapacity() < transforms.size()
        || resources.modelInverseTransposes->stagingCapacity()
               < transforms.size())
    {
        auto const bufferSize{static_cast<VkDeviceSize>(transforms.size())};

        resources.models = std::make_unique<TStagedBuffer<glm::mat4x4>>(
            TStagedBuffer<glm::mat4x4>::allocate(
                device,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                allocator,
                bufferSize
            )
        );
        resources.modelInverseTransposes =
            std::make_unique<TStagedBuffer<glm::mat4x4>>(
                TStagedBuffer<glm::mat4x4>::allocate(
                    device,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    allocator,
                    bufferSize
                )
            );
    }

    std::span<glm::mat4x4> const models{resources.models->mapFullCapacity()};
    std::span<glm::mat4x4> const modelInverseTransposes{
        resources.modelInverseTransposes->mapFullCapacity()
    };

    resources.models->resizeStaged(transforms.size());
    resources.modelInverseTransposes->resizeStaged(transforms.size());

    for (size_t index{0}; index < transforms.size(); index++)
    {
        syzygy::Transform const& transform{transforms[index]};

        glm::mat4x4 const model{worldMatrix * transform.toMatrix()};
        models[index] = model;
        modelInverseTransposes[index] = glm::inverseTranspose(model);
    }

    if (m_surfaceDescriptorsDirty)
    {
        while (resources.surfaceDescriptors.size() < mesh.surfaces.size())
        {
            std::optional<MaterialDescriptors> descriptorsResult{
                MaterialDescriptors::create(device, descriptorAllocator)
            };
            if (!descriptorsResult.has_value())
            {
                SZG_ERROR(
                    "Failed to allocate MaterialDescriptors while setting mesh."
                );
                return std::nullopt;
            }

            resources.surfaceDescriptors.push_back(
                std::move(descriptorsResult).value()
            );
        }

        resources.surfaceMaterialOverrides.resize(mesh.surfaces.size());

        for (size_t index{0}; index < mesh.surfaces.size(); index++)
        {
            GeometrySurface const& surface{mesh.surfaces[index]};
            MaterialDescriptors const& descriptors{
                resources.surfaceDescriptors[index]
            };
            MaterialData const& overrides{
                resources.surfaceMaterialOverrides[index]
            };

            MaterialData const activeMaterials{
                .ORM = overrides.ORM.lock() != nullptr ? overrides.ORM
                                                       : surface.material.ORM,
                .normal = overrides.normal.lock() != nullptr
                            ? overrides.normal
                            : surface.material.normal,
                .color = overrides.color.lock() != nullptr
                           ? overrides.color
                           : surface.material.color,
            };

            descriptors.write(activeMaterials);
        }

        m_surfaceDescriptorsDirty = false;
    }

    return resources;
}

auto MeshInstanced::create(
    std::optional<AssetPtr<Mesh>> const& mesh,
    InstanceAnimation const animation,
    std::string const& name,
    std::span<Transform const> const transforms,
    bool const castsShadow
) -> std::unique_ptr<MeshInstanced>
{
    auto result{std::make_unique<MeshInstanced>()};
    MeshInstanced& instance{*result};
    instance.render = true;
    instance.castsShadow = castsShadow;
    // TODO: name deduplication
    instance.name = fmt::format("meshInstanced_{}", name);

    if (mesh.has_value())
    {
        instance.setMesh(mesh.value());
    }

    instance.animation = animation;

    instance.originals.insert(
        instance.originals.begin(), transforms.begin(), transforms.end()
    );
    instance.transforms.insert(
        instance.transforms.begin(), transforms.begin(), transforms.end()
    );

    return result;
}

auto MeshInstanced::getMesh() const -> std::optional<AssetRef<Mesh>>
{
    std::shared_ptr<Asset<Mesh> const> const meshAsset{
        m_renderResources->mesh.lock()
    };
    if (meshAsset == nullptr)
    {
        return std::nullopt;
    }

    return *meshAsset;
}

auto MeshInstanced::getMaterialOverrides() const
    -> std::span<MaterialData const>
{
    std::shared_ptr<Asset<Mesh> const> const meshAsset{m_renderResources->mesh};
    if (meshAsset == nullptr || meshAsset->data == nullptr)
    {
        return {};
    }

    m_renderResources->surfaceMaterialOverrides.resize(
        m_renderResources->mesh.lock()->data->surfaces.size()
    );

    return std::span<MaterialData const>{
        m_renderResources->surfaceMaterialOverrides.begin(),
        m_renderResources->surfaceMaterialOverrides.begin()
            + static_cast<std::int64_t>(meshAsset->data->surfaces.size())
    };
}

void MeshInstanced::setMaterialOverrides(
    size_t const surface, MaterialData const& materialOverride
)
{
    m_surfaceDescriptorsDirty = true;

    if (surface >= m_renderResources->surfaceMaterialOverrides.size())
    {
        m_renderResources->surfaceMaterialOverrides.resize(surface + 1);
    }

    m_renderResources->surfaceMaterialOverrides[surface] = materialOverride;
}
} // namespace syzygy