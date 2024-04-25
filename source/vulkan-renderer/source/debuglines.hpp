#pragma once

#include "enginetypes.hpp"
#include "geometrystatics.hpp"

struct DebugLines
{
    std::unique_ptr<DebugLineComputePipeline> pipeline{};

    std::unique_ptr<TStagedBuffer<Vertex>> vertices{};
    std::unique_ptr<TStagedBuffer<uint32_t>> indices{};

    DrawResultsGraphics lastFrameDrawResults{};

    bool enabled{ false };
    float lineWidth{ 1.0 };

    void clear()
    {
        vertices->clearStaged();
        indices->clearStaged();
    }

    void push(glm::vec3 start, glm::vec3 end)
    {
        Vertex const startVertex{
            .position{ start },
            .uv_x{ 0.0 },
            .normal{ glm::vec3(0.0) },
            .uv_y{ 0.0 },
            .color{ glm::vec4(1.0,0.0,0.0,1.0) },
        };
        Vertex const endVertex{
            .position{ end },
            .uv_x{ 1.0 },
            .normal{ glm::vec3(0.0) },
            .uv_y{ 0.0 },
            .color{ glm::vec4(0.0,0.0,1.0,1.0) },
        };

        uint32_t const index{ static_cast<uint32_t>(indices->stagedSize()) };

        vertices->push(std::initializer_list{ startVertex, endVertex });
        indices->push(std::initializer_list{ index, index + 1 });
    }

    /*
    * Adds 4 line segmants defined by AB, BC, CD, DA. 
    * Winding does not matter since these are line segments.
    */
    void pushQuad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d)
    {
        push(a, b);
        push(b, c);
        push(c, d);
        push(d, a);
    }

    /*
    * Push a rectangle with possibly non-axis-aligned extents.
    */
    void pushRectangle(glm::vec3 center, glm::vec3 extentA, glm::vec3 extentB)
    {
        pushQuad(
            center + extentA + extentB
            , center + extentA - extentB
            , center - extentA - extentB
            , center - extentA + extentB
        );
    }

    /*
    * Push a rectangle, stretched along the (x,z) axes by extents.
    */
    void pushRectangle(glm::vec3 center, glm::quat orientation, glm::vec2 extents)
    {
        glm::vec3 scale{ extents.x, 1.0f, extents.y };

        glm::vec3 right{ orientation * (scale * geometry::right) };
        glm::vec3 forward{ orientation * (scale * geometry::forward) };

        pushRectangle(center, right, forward);
    }

    /*
    * Push a rectangular prism, stretched along the (x,y,z) axes by extents.
    */
    void pushBox(glm::vec3 center, glm::quat orientation, glm::vec3 extents)
    {
        glm::vec3 right{ orientation * (extents * geometry::right) };
        glm::vec3 forward{ orientation * (extents * geometry::forward) };
        glm::vec3 up{ orientation * (extents * geometry::up) };

        pushRectangle(center - up, right, forward);
        pushRectangle(center + up, right, forward);

        pushRectangle(center - right, forward, up);
        pushRectangle(center + right, forward, up);

        pushRectangle(center - forward, up, right);
        pushRectangle(center + forward, up, right);
    }

    void recordCopy(VkCommandBuffer cmd, VmaAllocator allocator)
    {
        vertices->recordCopyToDevice(cmd, allocator);
        indices->recordCopyToDevice(cmd, allocator);
    }
    void cleanup(VkDevice device, VmaAllocator allocator)
    {
        pipeline->cleanup(device);
        pipeline.reset();
        vertices.reset();
        indices.reset();
    }
};