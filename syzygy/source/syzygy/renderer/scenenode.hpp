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
    auto parent() -> std::optional<std::reference_wrapper<SceneNode>>;
    auto hasChildren() const -> bool;
    auto children() -> std::span<std::unique_ptr<SceneNode> const>;

    auto appendChild(std::string const& name = "") -> SceneNode&;

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
    std::vector<std::unique_ptr<SceneNode>> m_children{};
    std::unique_ptr<MeshInstanced> m_mesh{};
};
} // namespace syzygy