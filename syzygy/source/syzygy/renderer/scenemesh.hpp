#pragma once

#include "syzygy/assets/assetstypes.hpp"
#include "syzygy/assets/mesh.hpp"
#include "syzygy/geometry/transform.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/material.hpp"
#include <functional>
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace syzygy
{
struct DescriptorAllocator;
} // namespace syzygy

namespace syzygy
{
// Some hardcoded animations for demo purposes
enum class InstanceAnimation
{
    None,
    FIRST = None,

    Diagonal_Wave,

    Spin_Along_World_Up,
    LAST = Spin_Along_World_Up
};

struct MeshRenderResources
{
    bool castsShadow{true};

    std::unique_ptr<TStagedBuffer<glm::mat4x4>> models{};
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> modelInverseTransposes{};

    // The mesh will use the materials in this structure first, then defer to
    // the base asset's materials.
    std::vector<MaterialData> surfaceMaterialOverrides{};
    AssetPtr<Mesh> mesh{};
    std::vector<MaterialDescriptors> surfaceDescriptors{};
};

// TODO: encapsulate all fields
// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct MeshInstanced
{
    bool render{false};
    bool castsShadow{true};
    std::string name{};

    InstanceAnimation animation{InstanceAnimation::None};

    // This transform data + gpu buffers requires manual management for now

    std::vector<Transform> originals{};
    std::vector<Transform> transforms{};

    [[nodiscard]] auto getMesh() const -> std::optional<AssetRef<Mesh>>;
    void setMesh(AssetPtr<Mesh>);

    auto prepareForRendering(
        VkDevice,
        VmaAllocator,
        DescriptorAllocator&,
        glm::mat4x4 const& worldMatrix
    ) -> std::optional<std::reference_wrapper<MeshRenderResources>>;

    [[nodiscard]] static auto create(
        std::optional<AssetPtr<Mesh>> const& mesh,
        InstanceAnimation animation,
        std::string const& name,
        std::span<Transform const> transforms,
        bool castsShadow = true
    ) -> std::unique_ptr<MeshInstanced>;

    // Returns only as many overrides as there are surfaces in the current mesh
    // May return empty if no overrides are initialized.
    // TODO: this is only used for UI right now, which is sort of annoying,
    // there must be a better way to organize this data flow
    [[nodiscard]] auto getMaterialOverrides() const
        -> std::span<MaterialData const>;
    void setMaterialOverrides(size_t surface, MaterialData const&);

private:
    bool m_surfaceDescriptorsDirty{false};

    std::unique_ptr<MeshRenderResources> m_renderResources{};
};
// NOLINTEND(misc-non-private-member-variables-in-classes)
} // namespace syzygy