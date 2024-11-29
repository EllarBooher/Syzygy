#pragma once

#include "syzygy/core/input.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/platform/integer.hpp"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>

namespace syzygy
{
struct InputSnapshot;
struct UIRectangle;
struct Camera;
struct SceneNode;
struct WireframeOverlay;
} // namespace syzygy

// TODO: add options to transform scale and rotation. Also display bounding
// boxes and any other useful things that might come up.

namespace syzygy
{
// A persistant gizmo that references a single scene node, renders useful
// information, and takes in mouse controls to manipulate the transform of this
// node.
// For now, only supports manipulating the translation.
struct SceneNodeGizmo
{
    void handleCursor(
        InputSnapshot const& input,
        UIRectangle const& sceneBoundary,
        Camera const& editorPOV
    );

    void render(WireframeOverlay&);

    void setNode(std::weak_ptr<SceneNode>);

    // TODO: This Ad-hoc focus/event consumption needs to be generalized to
    // be easier on integration

    [[nodiscard]] auto consumedCursor() -> bool;

private:
    std::weak_ptr<SceneNode> m_selectedNode;

    // 0 = X, 1 = Y, 2 = Z. Normally capped at 2 (for 3 axes).
    // This value matters when consuming cursor inputs, to decide which way to
    // move the scene node.
    std::optional<size_t> m_manipulatedAxis{};

    // The position the cursor started at
    std::optional<glm::vec2> m_cursorDragScreenStart{};

    // The position the object started at
    std::optional<glm::vec3> m_positionDragWorldStart{};

    Ray m_cursorRay{};

    bool m_released{};
};
} // namespace syzygy