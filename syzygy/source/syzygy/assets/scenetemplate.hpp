#pragma once

#include "syzygy/assets/assetstypes.hpp"
#include "syzygy/assets/mesh.hpp"
#include "syzygy/geometry/transform.hpp"
#include "syzygy/renderer/scenenode.hpp"
#include <memory>
#include <vector>

namespace syzygy
{
struct SceneTemplateNode
{
    Transform transform;
    std::optional<AssetPtr<Mesh>> mesh;
    std::vector<size_t> children;
    std::string name;
};

struct SceneTemplate
{
    // Creates all the nodes of this template, appended to the given node which
    // is treated as the root of this template's scene.
    void appendTo(SceneNode&) const;

    static auto create(std::vector<SceneTemplateNode>&& nodes) -> SceneTemplate
    {
        SceneTemplate result{};
        result.m_nodes = std::move(nodes);
        return result;
    }

private:
    std::vector<SceneTemplateNode> m_nodes;
};
} // namespace syzygy