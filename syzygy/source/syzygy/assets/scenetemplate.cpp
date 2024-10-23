#include "scenetemplate.hpp"

namespace detail
{
void visitNode(
    syzygy::SceneNode& parent,
    std::vector<syzygy::SceneTemplateNode> const& nodes,
    syzygy::SceneTemplateNode const& source
)
{
    syzygy::SceneNode& node{parent.appendChild(source.name)};
    node.transform = source.transform;
    if (source.mesh.has_value())
    {
        std::array<syzygy::Transform, 1> transforms{syzygy::Transform::identity(
        )};

        node.swapMesh(syzygy::MeshInstanced::create(
            source.mesh.value(),
            syzygy::InstanceAnimation::None,
            node.name(),
            transforms
        ));
    }

    for (size_t const childIndex : source.children)
    {
        syzygy::SceneTemplateNode const& child{nodes[childIndex]};

        visitNode(node, nodes, child);
    }
}
} // namespace detail

namespace syzygy
{
void SceneTemplate::appendTo(SceneNode& root) const
{
    detail::visitNode(root, m_nodes, m_nodes[0]);
}
} // namespace syzygy
