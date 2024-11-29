#include "scenenodegizmo.hpp"
#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/renderer/pipelines/wireframeoverlay.hpp"
#include "syzygy/renderer/scene.hpp"
#include "syzygy/renderer/scenenode.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include <glm/gtx/projection.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/vec_swizzle.hpp>

namespace detail
{
float constexpr AXIS_MINOR_RADIUS{0.1F};
float constexpr AXIS_MAJOR_LENGTH{2.0F};
syzygy::AABB constexpr X_AXIS_HITBOX{
    .center = glm::vec3{0.5F * AXIS_MAJOR_LENGTH, 0.0F, 0.0F},
    .halfExtent =
        glm::vec3{
            0.5F * AXIS_MAJOR_LENGTH, AXIS_MINOR_RADIUS, AXIS_MINOR_RADIUS
        },
};
// Should use glm vec_swizzle, but methods are not constexpr
syzygy::AABB constexpr Y_AXIS_HITBOX{
    .center =
        glm::vec3{
            X_AXIS_HITBOX.center.z,
            X_AXIS_HITBOX.center.x,
            X_AXIS_HITBOX.center.y
        },
    .halfExtent =
        glm::vec3{
            X_AXIS_HITBOX.halfExtent.z,
            X_AXIS_HITBOX.halfExtent.x,
            X_AXIS_HITBOX.halfExtent.y
        },
};
syzygy::AABB constexpr Z_AXIS_HITBOX{
    .center =
        glm::vec3{
            X_AXIS_HITBOX.center.y,
            X_AXIS_HITBOX.center.z,
            X_AXIS_HITBOX.center.x
        },
    .halfExtent =
        glm::vec3{
            X_AXIS_HITBOX.halfExtent.y,
            X_AXIS_HITBOX.halfExtent.z,
            X_AXIS_HITBOX.halfExtent.x
        },
};

} // namespace detail

namespace syzygy
{
void SceneNodeGizmo::handleCursor(
    InputSnapshot const& input,
    UIRectangle const& sceneBoundary,
    Camera const& editorPOV
)
{
    std::shared_ptr<SceneNode> const selectedNode{m_selectedNode.lock()};
    if (selectedNode == nullptr)
    {
        return;
    }

    if (!sceneBoundary.contains(glm::vec2{input.cursor.currentPosition}))
    {
        return;
    }

    std::optional<double> const aspectRatioResult{
        aspectRatio(sceneBoundary.size())
    };
    if (!aspectRatioResult.has_value())
    {
        SZG_WARNING("Scene Boundary had invalid aspect ratio.");
        return;
    }

    glm::mat4x4 const proj{editorPOV.projection(aspectRatioResult.value())};

    glm::vec3 const frameBufferCoord{
        input.cursor.currentPosition.x - sceneBoundary.pos().x,
        input.cursor.currentPosition.y - sceneBoundary.pos().y,
        1.0F
    };

    glm::vec4 const NDCSpaceCoord{
        2.0F * (frameBufferCoord.x / sceneBoundary.size().x) - 1.0F,
        2.0F * (frameBufferCoord.y / sceneBoundary.size().y) - 1.0F,
        frameBufferCoord.z,
        1.0F
    };

    // Need two positions to construct a direction vector
    // Could also transform a vector with w = 0
    glm::vec4 const unscaledViewPos{glm::inverse(proj) * NDCSpaceCoord};

    glm::vec3 const from{editorPOV.cameraPosition};
    glm::vec3 const to{
        editorPOV.transform() * (unscaledViewPos / unscaledViewPos.w)
    };
    m_cursorRay = Ray::create(from, to);
    m_cursorRay.direction = glm::normalize(m_cursorRay.direction);

    ButtonStatus const LMBStatus{input.cursor.getStatus(MouseButtonCode::LEFT)};

    m_released = false;

    bool const inDragEvent{m_cursorDragScreenStart.has_value()};
    glm::vec3 const nodePosition{selectedNode->transform.translation};
    if (!inDragEvent)
    {
        std::array<HitResult, 3> const axisHits{
            detail::X_AXIS_HITBOX.translate(nodePosition)
                .rayHitTest(m_cursorRay),
            detail::Y_AXIS_HITBOX.translate(nodePosition)
                .rayHitTest(m_cursorRay),
            detail::Z_AXIS_HITBOX.translate(nodePosition)
                .rayHitTest(m_cursorRay)
        };

        for (size_t axisIndex{0}; axisIndex < 3; axisIndex++)
        {
            if (axisHits[axisIndex].hit)
            {
                m_manipulatedAxis = axisIndex;
                break;
            }
            m_manipulatedAxis = std::nullopt;
        }

        if (LMBStatus.pressed() && m_manipulatedAxis.has_value())
        {
            m_cursorDragScreenStart = input.cursor.currentPosition;
            m_positionDragWorldStart = nodePosition;
        }
    }
    else if (!LMBStatus.down)
    {
        // Note that we don't check for released (!down && edge), since we might
        // somehow miss that specific frame
        m_cursorDragScreenStart = std::nullopt;
        m_positionDragWorldStart = std::nullopt;

        m_released = true;
    }
    else
    {
        // LMB is down and we are continuing to drag
        assert(m_manipulatedAxis.has_value());
        assert(m_positionDragWorldStart.has_value());

        glm::vec3 translationDirection{};
        translationDirection[m_manipulatedAxis.value()] = 1.0F;

        // The axis the user drags and the position gets snapped to, to reduce
        // the degrees of freedom and make control easier.
        Ray const translationAxis{
            .position = m_positionDragWorldStart.value(),
            .direction = glm::normalize(translationDirection),
        };

        glm::vec2 const translationDeltaNDC{glm::normalize(
            editorPOV.worldToNDC(
                translationAxis.at(0.0F), aspectRatioResult.value()
            )
            - editorPOV.worldToNDC(
                translationAxis.at(1.0F), aspectRatioResult.value()
            )
        )};

        glm::vec2 const dragDeltaNDC{
            2.0F
            * (glm::vec2{input.cursor.currentPosition}
               - m_cursorDragScreenStart.value())
            / sceneBoundary.size()
        };

        // The difficulty coming up is that as the user drags in screen space,
        // this does not translate linearly to world space, since the
        // translation axis turns into a curve in NDC/screen space coordinates.
        //
        // This means we have to find the closest position in screen/NDC space
        // since this is what the user intuits, then transform back to world
        // space.

        glm::vec2 const screenPositionOnTranslationAxisClosestToCursor{
            glm::vec2{editorPOV.worldToNDC(
                translationAxis.at(0.0F), aspectRatioResult.value()
            )}
            + glm::dot(dragDeltaNDC, translationDeltaNDC) * translationDeltaNDC
        };

        // This position is ambiguous/arbitrary, as most transformation from NDC
        // to world are, since we cannot easily guess the depth value needed.
        // Thus we turn it into a line and intersect this line with our original
        // translation axis.
        glm::vec3 const newPositionWorld{editorPOV.NDCToWorld(
            glm::vec3{screenPositionOnTranslationAxisClosestToCursor, 1.0F},
            aspectRatioResult.value()
        )};

        Ray const newPositionLine{
            .position = editorPOV.cameraPosition,
            .direction =
                glm::normalize(newPositionWorld - editorPOV.cameraPosition)
        };

        // this line should intersect our translation axis. Find where it
        // intersects by treating the translation axis as a plane instead.
        // Cross twice to get a normal that does not describe a plane containing
        // both lines.
        glm::vec3 const planeNormal{glm::cross(
            glm::cross(newPositionLine.direction, translationAxis.direction),
            translationAxis.direction
        )};

        if (glm::epsilonNotEqual(
                glm::dot(newPositionLine.direction, planeNormal),
                0.0F,
                glm::epsilon<float>()
            ))
        {
            // Is there any reasonable fallback for if the translation axis
            // lies on a perspective line from the camera?
            glm::vec3 const planeIntersection{newPositionLine.at(
                glm::dot(
                    (translationAxis.position - newPositionLine.position),
                    planeNormal
                )
                / glm::dot(newPositionLine.direction, planeNormal)
            )};

            selectedNode->transform.translation = planeIntersection;
        }
    }
}
void SceneNodeGizmo::render(WireframeOverlay& overlay)
{
    std::shared_ptr<SceneNode> const selectedNode{m_selectedNode.lock()};
    if (selectedNode == nullptr)
    {
        return;
    }

    glm::vec3 const nodePosition{selectedNode->transform.translation};

    bool const xManipulated{
        m_manipulatedAxis.has_value() && m_manipulatedAxis.value() == 0
    };
    overlay.pushArrow(
        Ray::create(
            nodePosition,
            nodePosition
                + detail::AXIS_MAJOR_LENGTH * glm::vec3{1.0F, 0.0F, 0.0F}
        ),
        {.layer = WireframeLayers::Gizmo,
         .colorRGB = xManipulated ? glm::vec3{1.0F, 0.25F, 0.25F}
                                  : glm::vec3{1.0F, 0.0F, 0.0F}}
    );
    bool const yManipulated{
        m_manipulatedAxis.has_value() && m_manipulatedAxis.value() == 1
    };
    overlay.pushArrow(
        Ray::create(
            nodePosition,
            nodePosition
                + detail::AXIS_MAJOR_LENGTH * glm::vec3{0.0F, 1.0F, 0.0F}
        ),
        {.layer = WireframeLayers::Gizmo,
         .colorRGB = yManipulated ? glm::vec3{0.25F, 1.0F, 0.25F}
                                  : glm::vec3{0.0F, 1.0F, 0.0F}}
    );
    bool const zManipulated{
        m_manipulatedAxis.has_value() && m_manipulatedAxis.value() == 2
    };
    overlay.pushArrow(
        Ray::create(
            nodePosition,
            nodePosition
                + detail::AXIS_MAJOR_LENGTH * glm::vec3{0.0F, 0.0F, 1.0F}
        ),
        {.layer = WireframeLayers::Gizmo,
         .colorRGB = zManipulated ? glm::vec3{0.25F, 0.25F, 1.0F}
                                  : glm::vec3{0.0F, 0.0F, 1.0F}}
    );
}
void SceneNodeGizmo::setNode(std::weak_ptr<SceneNode> const node)
{
    m_selectedNode = node;
}
auto SceneNodeGizmo::consumedCursor() -> bool
{
    return m_cursorDragScreenStart.has_value() || m_released;
}
} // namespace syzygy
