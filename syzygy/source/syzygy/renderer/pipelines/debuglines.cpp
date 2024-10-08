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

void DebugLines::push(glm::vec3 const start, glm::vec3 const end)
{
    VertexPacked const startVertex{
        .position = start,
        .uv_x = 0.0,
        .normal = glm::vec3(0.0),
        .uv_y = 0.0,
        .color = glm::vec4(1.0, 0.0, 0.0, 1.0),
    };
    VertexPacked const endVertex{
        .position = end,
        .uv_x = 1.0,
        .normal = glm::vec3(0.0),
        .uv_y = 0.0,
        .color = glm::vec4(0.0, 0.0, 1.0, 1.0),
    };

    uint32_t const index{static_cast<uint32_t>(indices->stagedSize())};

    vertices->push(std::initializer_list<VertexPacked>{startVertex, endVertex});
    indices->push(std::initializer_list<uint32_t>{index, index + 1});
}

// NOLINTEND(readability-make-member-function-const)

void DebugLines::pushQuad(
    glm::vec3 const a, glm::vec3 const b, glm::vec3 const c, glm::vec3 const d
)
{
    push(a, b);
    push(b, c);
    push(c, d);
    push(d, a);
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

void DebugLines::pushBox(Transform const parent, AABB const box)
{
    glm::mat4x4 const transformation{parent.toMatrix()};

    glm::vec3 const right{
        transformation * glm::vec4{box.halfExtent * WORLD_RIGHT, 0.0F}
    };
    glm::vec3 const forward{
        transformation * glm::vec4{box.halfExtent * WORLD_FORWARD, 0.0F}
    };
    glm::vec3 const up{
        transformation * glm::vec4{box.halfExtent * WORLD_UP, 0.0F}
    };

    glm::vec3 const center{transformation * glm::vec4{box.center, 1.0F}};

    pushRectangleAxes(center - up, right, forward);
    pushRectangleAxes(center + up, right, forward);

    pushRectangleAxes(center - right, forward, up);
    pushRectangleAxes(center + right, forward, up);

    pushRectangleAxes(center - forward, up, right);
    pushRectangleAxes(center + forward, up, right);
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