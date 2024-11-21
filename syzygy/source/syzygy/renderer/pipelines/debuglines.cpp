#include "debuglines.hpp"

#include "syzygy/core/timing.hpp"
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

void DebugLines::push(
    glm::vec3 const start, glm::vec3 const end, DebugLinesArguments const args
)
{
    m_segments.push_back(DebugLineSegment{
        .start =
            {
                .position = start,
                .uv_x = 0.0,
                .normal = glm::vec3(0.0),
                .uv_y = 0.0,
                .color = glm::vec4(args.colorRGB, 1.0),
            },
        .end =
            {
                .position = end,
                .uv_x = 1.0,
                .normal = glm::vec3(0.0),
                .uv_y = 0.0,
                .color = glm::vec4(args.colorRGB, 1.0),
            },
        .endTimeSeconds = m_currentFrameStartSeconds + args.lifetimeSeconds
    });
}

void DebugLines::push(
    glm::vec3 const localStart,
    glm::vec3 const localEnd,
    glm::mat4x4 const worldMatrix,
    DebugLinesArguments const args
)
{
    push(
        glm::vec3{worldMatrix * glm::vec4{localStart, 1.0F}},
        glm::vec3{worldMatrix * glm::vec4{localEnd, 1.0F}},
        args
    );
}

// NOLINTEND(readability-make-member-function-const)

void DebugLines::pushQuad(
    glm::vec3 const a,
    glm::vec3 const b,
    glm::vec3 const c,
    glm::vec3 const d,
    DebugLinesArguments const args
)
{
    push(a, b, args);
    push(b, c, args);
    push(c, d, args);
    push(d, a, args);
}

void DebugLines::pushArrow(
    Transform const transform,
    float const length,
    DebugLinesArguments const args
)
{
    float constexpr ARROW_HEAD_RADIUS{0.1F};
    glm::vec3 constexpr ARROW_HEAD_POSITION{0.0F, 0.0F, 1.0F};
    std::array<glm::vec3, 4> constexpr ARROW_HEAD_VERTICES{
        -ARROW_HEAD_RADIUS * glm::vec3{1.0F, 1.0F, 1.0F},
        -ARROW_HEAD_RADIUS * glm::vec3{-1.0F, 1.0F, 1.0F},
        -ARROW_HEAD_RADIUS * glm::vec3{-1.0F, -1.0F, 1.0F},
        -ARROW_HEAD_RADIUS * glm::vec3{1.0F, -1.0F, 1.0F},
    };

    glm::mat4x4 const worldMatrix{transform.toMatrix()};

    // Arrow body
    push(glm::vec3{0.0F}, ARROW_HEAD_POSITION * length, worldMatrix, args);

    // Arrow head
    for (size_t vertexIndex{0}; vertexIndex < ARROW_HEAD_VERTICES.size();
         vertexIndex++)
    {
        // Tip to arrowhead circumferance
        push(
            ARROW_HEAD_POSITION * length,
            ARROW_HEAD_POSITION * length + ARROW_HEAD_VERTICES[vertexIndex],
            worldMatrix,
            args
        );
        // Arrowhead circumferance
        push(
            ARROW_HEAD_POSITION * length + ARROW_HEAD_VERTICES[vertexIndex],
            ARROW_HEAD_POSITION * length
                + ARROW_HEAD_VERTICES
                    [(vertexIndex + 1) % ARROW_HEAD_VERTICES.size()],
            worldMatrix,
            args
        );
    }
}

void DebugLines::pushArrow(Ray const ray, DebugLinesArguments const args)
{
    pushArrow(
        Transform::lookAt(ray, glm::vec3{1.0F}),
        glm::length(ray.direction),
        args
    );
}

void DebugLines::pushRectangleAxes(
    glm::vec3 const center,
    glm::vec3 const extentA,
    glm::vec3 const extentB,
    DebugLinesArguments const args
)
{
    pushQuad(
        center + extentA + extentB,
        center + extentA - extentB,
        center - extentA - extentB,
        center - extentA + extentB,
        args
    );
}

void DebugLines::pushRectangleOriented(
    glm::vec3 const center,
    glm::quat const orientation,
    glm::vec2 const extents,
    DebugLinesArguments const args
)
{
    glm::vec3 const scale{extents.x, 1.0F, extents.y};

    glm::vec3 const right{orientation * (scale * WORLD_RIGHT)};
    glm::vec3 const forward{orientation * (scale * WORLD_FORWARD)};

    pushRectangleAxes(center, right, forward, args);
}

void DebugLines::pushBox(
    glm::vec3 const center,
    glm::quat const orientation,
    glm::vec3 const extents,
    DebugLinesArguments const args
)
{
    glm::vec3 const right{orientation * (extents * WORLD_RIGHT)};
    glm::vec3 const forward{orientation * (extents * WORLD_FORWARD)};
    glm::vec3 const up{orientation * (extents * WORLD_UP)};

    pushRectangleAxes(center - up, right, forward, args);
    pushRectangleAxes(center + up, right, forward, args);

    pushRectangleAxes(center - right, forward, up, args);
    pushRectangleAxes(center + right, forward, up, args);

    pushRectangleAxes(center - forward, up, right, args);
    pushRectangleAxes(center + forward, up, right, args);
}

void DebugLines::pushBox(
    glm::mat4x4 const parent, AABB const box, DebugLinesArguments const args
)
{
    glm::vec3 const right{
        parent * glm::vec4{box.halfExtent * WORLD_RIGHT, 0.0F}
    };
    glm::vec3 const forward{
        parent * glm::vec4{box.halfExtent * WORLD_FORWARD, 0.0F}
    };
    glm::vec3 const up{parent * glm::vec4{box.halfExtent * WORLD_UP, 0.0F}};

    glm::vec3 const center{parent * glm::vec4{box.center, 1.0F}};

    pushRectangleAxes(center - up, right, forward, args);
    pushRectangleAxes(center + up, right, forward, args);

    pushRectangleAxes(center - right, forward, up, args);
    pushRectangleAxes(center + right, forward, up, args);

    pushRectangleAxes(center - forward, up, right, args);
    pushRectangleAxes(center + forward, up, right, args);
}

void DebugLines::pushBox(
    Transform const parent, AABB const box, DebugLinesArguments const args
)
{
    glm::mat4x4 const transformation{parent.toMatrix()};

    pushBox(transformation, box, args);
}

void DebugLines::clear() { m_segments.clear(); }

void DebugLines::tick(TickTiming const& timing)
{
    std::erase_if(
        m_segments,
        [=](DebugLineSegment const& segment)
    { return segment.endTimeSeconds <= timing.timeElapsedSeconds; }
    );

    m_currentFrameStartSeconds = timing.timeElapsedSeconds;
}

auto DebugLines::empty() const -> bool { return m_segments.empty(); }

void DebugLines::recordCopy(VkCommandBuffer const cmd) const
{
    // Possibly expensive, since this synchronously copies the data onto a
    // staging buffer.

    vertices->clearStaged();
    indices->clearStaged();

    // Can be done way better by copying spans of data at once, and
    // splitting up the segment vertices and ages. Plus, we know the index
    // buffer just needs to be 0, 1, 2, 3, ... etc right now.
    for (auto const& segment : m_segments)
    {
        uint32_t const index{static_cast<uint32_t>(indices->stagedSize())};

        vertices->push(
            std::initializer_list<VertexPacked>{segment.start, segment.end}
        );
        indices->push(std::initializer_list<uint32_t>{index, index + 1});
    }

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