#pragma once

#include "enginetypes.hpp"
#include "geometrystatics.hpp"
#include "pipelines.hpp"

struct DebugLines
{
    std::unique_ptr<DebugLineGraphicsPipeline> pipeline{};

    std::unique_ptr<TStagedBuffer<Vertex>> vertices{};
    std::unique_ptr<TStagedBuffer<uint32_t>> indices{};

    DrawResultsGraphics lastFrameDrawResults{};

    bool enabled{ false };
    float lineWidth{ 1.0 };

    void clear();

    void push(
        glm::vec3 start
        , glm::vec3 end
    );

    // Adds 4 line segmants defined by AB, BC, CD, DA. 
    // Winding does not matter since these are added as separate line segments.
    void pushQuad(
        glm::vec3 a
        , glm::vec3 b
        , glm::vec3 c
        , glm::vec3 d
    );

    // Push a rectangle with possibly non-axis-aligned extents.
    void pushRectangleAxes(
        glm::vec3 center
        , glm::vec3 extentA
        , glm::vec3 extentB
    );

    // Push a rectangle, stretched along the (x,z) axes by extents.
    void pushRectangleOriented(
        glm::vec3 center
        , glm::quat orientation
        , glm::vec2 extents
    );

    // Push a rectangular prism, stretched along the (x,y,z) axes by extents.
    void pushBox(
        glm::vec3 center
        , glm::quat orientation
        , glm::vec3 extents
    );

    void recordCopy(
        VkCommandBuffer cmd
        , VmaAllocator allocator
    );

    void cleanup(
        VkDevice device
        , VmaAllocator allocator
    );
};