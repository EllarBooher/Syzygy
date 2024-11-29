#pragma once

#include "syzygy/geometry/transform.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/renderer/scenemesh.hpp"
#include <algorithm>
#include <functional>
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <span>
#include <stack>
#include <string>
#include <vector>

namespace syzygy
{
struct SceneNode;
struct SceneIterator
{
    using difference_type = std::ptrdiff_t;
    using value_type = SceneNode;
    using pointer = value_type*;
    using reference = value_type&;

    SceneIterator() = default;
    SceneIterator(pointer ptr);

    auto operator*() const -> reference;

    auto operator++() -> SceneIterator&;
    auto operator++(int) -> SceneIterator;

    auto operator==(SceneIterator const&) const -> bool;

private:
    pointer m_ptr{};
    size_t m_siblingIndex{};
    std::stack<size_t> m_path{};
};
static_assert(std::forward_iterator<SceneIterator>);

struct SceneNode
{
    // TODO: error handling for SceneNode interface. These methods are written
    // to protect a tree's invariants, but for now these methods just throw upon
    // invalid inputs.

    [[nodiscard]] auto parent()
        -> std::optional<std::reference_wrapper<SceneNode>>;
    [[nodiscard]] auto hasChildren() const -> bool;
    [[nodiscard]] auto children()
        -> std::span<std::shared_ptr<SceneNode> const>;

    [[nodiscard]] auto childrenCount() const -> size_t;
    [[nodiscard]] auto childAt(size_t index) const -> SceneNode const&;

    [[nodiscard]] auto refSelf() const -> std::weak_ptr<SceneNode>;
    [[nodiscard]] auto refChild(SceneNode const&) const
        -> std::weak_ptr<SceneNode>;

    [[nodiscard]] auto descendent(SceneNode const*) const -> bool;

    // Creates a free-standing node without children.
    static auto create(std::string&& name) -> std::unique_ptr<SceneNode>;

    // Adds a new child to the end of this nodes list of children
    auto createChild(std::string&& name) -> SceneNode&;

    // Adds an existing node to the end of this nodes list of children, removing
    // it from its parent first if possible.
    // Asserts upon the following invalid states: newChild is null, or this node
    // has newChild in its parent chain.
    void appendChild(std::shared_ptr<SceneNode> newChild);

    // Removes the child from this node. Asserts if the passed node is not a
    // child.
    auto removeChild(SceneNode&) -> std::shared_ptr<SceneNode>;

    // Removes the child from this node, while donating its children. Asserts if
    // the passed node is not a child.
    auto extractChild(SceneNode&) -> std::shared_ptr<SceneNode>;

    // If this node has a parent, tries to remove itself from it. Returns
    // nullptr if this node has no parent.
    auto tryRemoveSelf() -> std::shared_ptr<SceneNode>;
    // If this node has a parent, tries to extract itself from it. Returns
    // nullptr if this node has no parent.
    auto tryExtractSelf() -> std::shared_ptr<SceneNode>;

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    Transform transform{Transform::identity()};

    [[nodiscard]] auto depth() const -> size_t;
    // Returns the transformation matrix up the scene hierarchy INCLUDING this
    // transform.
    [[nodiscard]] auto transformToRoot() const -> glm::mat4x4;

    [[nodiscard]] auto name() const -> std::string const&;

    [[nodiscard]] auto accessMesh()
        -> std::optional<std::reference_wrapper<MeshInstanced>>;
    [[nodiscard]] auto accessMesh() const
        -> std::optional<std::reference_wrapper<MeshInstanced const>>;
    auto swapMesh(std::unique_ptr<MeshInstanced>)
        -> std::unique_ptr<MeshInstanced>;

    [[nodiscard]] auto begin() -> SceneIterator;
    [[nodiscard]] auto end() -> SceneIterator;

private:
    SceneNode* m_parent{};
    std::string m_name{};
    std::vector<std::shared_ptr<SceneNode>> m_children{};
    std::unique_ptr<MeshInstanced> m_mesh{};
};
} // namespace syzygy