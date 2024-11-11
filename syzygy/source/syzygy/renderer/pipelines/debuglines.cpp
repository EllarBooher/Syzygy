#include "debuglines.hpp"

#include "syzygy/geometry/geometrystatics.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/geometry/transform.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <initializer_list>
#include <span>

namespace syzygy
{

// NOLINTBEGIN(readability-make-member-function-const)

void DebugLines::clear()
{
    vertices->clearStaged();
    indices->clearStaged();
}

void DebugLines::push(
    glm::vec3 const start, glm::vec3 const end, glm::vec3 const colorRGB
)
{
    VertexPacked const startVertex{
        .position = start,
        .uv_x = 0.0,
        .normal = glm::vec3(0.0),
        .uv_y = 0.0,
        .color = glm::vec4(colorRGB, 1.0),
    };
    VertexPacked const endVertex{
        .position = end,
        .uv_x = 1.0,
        .normal = glm::vec3(0.0),
        .uv_y = 0.0,
        .color = glm::vec4(colorRGB, 1.0),
    };

    uint32_t const index{static_cast<uint32_t>(indices->stagedSize())};

    vertices->push(std::initializer_list<VertexPacked>{startVertex, endVertex});
    indices->push(std::initializer_list<uint32_t>{index, index + 1});
}

void DebugLines::push(
    glm::vec3 const localStart,
    glm::vec3 const localEnd,
    glm::mat4x4 const worldMatrix,
    glm::vec3 const colorRGB
)
{
    push(
        glm::vec3{worldMatrix * glm::vec4{localStart, 1.0F}},
        glm::vec3{worldMatrix * glm::vec4{localEnd, 1.0F}},
        colorRGB
    );
}

// NOLINTEND(readability-make-member-function-const)

void DebugLines::pushQuad(
    glm::vec3 const a, glm::vec3 const b, glm::vec3 const c, glm::vec3 const d
)
{
    push(a, b, glm::vec3{0.0, 1.0, 0.0});
    push(b, c, glm::vec3{0.0, 1.0, 0.0});
    push(c, d, glm::vec3{0.0, 1.0, 0.0});
    push(d, a, glm::vec3{0.0, 1.0, 0.0});
}

void DebugLines::pushArrow(Transform const transform, glm::vec3 const colorRGB)
{
    float constexpr ARROW_HEAD_RADIUS{0.1F};
    glm::vec3 constexpr ARROW_HEAD_POSITION{0.0F, 0.0F, 1.0F};
    std::array<glm::vec3, 4> constexpr ARROW_HEAD_VERTICES{
        ARROW_HEAD_POSITION - ARROW_HEAD_RADIUS * glm::vec3{1.0F, 1.0F, 1.0F},
        ARROW_HEAD_POSITION - ARROW_HEAD_RADIUS * glm::vec3{-1.0F, 1.0F, 1.0F},
        ARROW_HEAD_POSITION - ARROW_HEAD_RADIUS * glm::vec3{-1.0F, -1.0F, 1.0F},
        ARROW_HEAD_POSITION - ARROW_HEAD_RADIUS * glm::vec3{1.0F, -1.0F, 1.0F},
    };

    glm::mat4x4 const worldMatrix{transform.toMatrix()};

    // Arrow body
    push(glm::vec3{0.0F}, ARROW_HEAD_POSITION, worldMatrix, colorRGB);

    // Arrow head
    for (size_t vertexIndex{0}; vertexIndex < ARROW_HEAD_VERTICES.size();
         vertexIndex++)
    {
        // Tip to arrowhead circumferance
        push(
            ARROW_HEAD_POSITION,
            ARROW_HEAD_VERTICES[vertexIndex],
            worldMatrix,
            colorRGB
        );
        // Arrowhead circumferance
        push(
            ARROW_HEAD_VERTICES[vertexIndex],
            ARROW_HEAD_VERTICES[(vertexIndex + 1) % ARROW_HEAD_VERTICES.size()],
            worldMatrix,
            colorRGB
        );
    }
}

void DebugLines::pushRectangleAxes(
    glm::vec3 const center, glm::vec3 const extentA, glm::vec3 const extentB
)
{
    pushQuad(
        center + extentA + extentB,
        center + extentA - extentB,
        center - extentA - extentB,
        center - extentA + extentB
    );
}

void DebugLines::pushRectangleOriented(
    glm::vec3 const center, glm::quat const orientation, glm::vec2 const extents
)
{
    glm::vec3 const scale{extents.x, 1.0F, extents.y};

    glm::vec3 const right{orientation * (scale * WORLD_RIGHT)};
    glm::vec3 const forward{orientation * (scale * WORLD_FORWARD)};

    pushRectangleAxes(center, right, forward);
}

void DebugLines::pushBox(
    glm::vec3 const center, glm::quat const orientation, glm::vec3 const extents
)
{
    glm::vec3 const right{orientation * (extents * WORLD_RIGHT)};
    glm::vec3 const forward{orientation * (extents * WORLD_FORWARD)};
    glm::vec3 const up{orientation * (extents * WORLD_UP)};

    pushRectangleAxes(center - up, right, forward);
    pushRectangleAxes(center + up, right, forward);

    pushRectangleAxes(center - right, forward, up);
    pushRectangleAxes(center + right, forward, up);

    pushRectangleAxes(center - forward, up, right);
    pushRectangleAxes(center + forward, up, right);
}

void DebugLines::pushBox(glm::mat4x4 const parent, AABB const box)
{
    glm::vec3 const right{
        parent * glm::vec4{box.halfExtent * WORLD_RIGHT, 0.0F}
    };
    glm::vec3 const forward{
        parent * glm::vec4{box.halfExtent * WORLD_FORWARD, 0.0F}
    };
    glm::vec3 const up{parent * glm::vec4{box.halfExtent * WORLD_UP, 0.0F}};

    glm::vec3 const center{parent * glm::vec4{box.center, 1.0F}};

    pushRectangleAxes(center - up, right, forward);
    pushRectangleAxes(center + up, right, forward);

    pushRectangleAxes(center - right, forward, up);
    pushRectangleAxes(center + right, forward, up);

    pushRectangleAxes(center - forward, up, right);
    pushRectangleAxes(center + forward, up, right);
}

void DebugLines::pushBox(Transform const parent, AABB const box)
{
    glm::mat4x4 const transformation{parent.toMatrix()};

    pushBox(transformation, box);
}

void DebugLines::recordCopy(VkCommandBuffer const cmd) const
{
    vertices->recordCopyToDevice(cmd);
    indices->recordCopyToDevice(cmd);
}

void DebugLines::cleanup(
    VkDevice const device, VmaAllocator const /*allocator*/
)
{
    pipeline->cleanup(device);
    pipeline.reset();
    vertices.reset();
    indices.reset();
}
} // namespace syzygy