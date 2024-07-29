#pragma once

#include "syzygy/buffers.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/pipelines.hpp"
#include "syzygy/vulkanusage.hpp"
#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>

struct Vertex;

struct DebugLines
{
    // TODO: Split this up into 3 segments: the pipeline, the line segment
    // buffers, and the configuration.
public:
    std::unique_ptr<TStagedBuffer<Vertex>> vertices{};
    std::unique_ptr<TStagedBuffer<uint32_t>> indices{};

    std::unique_ptr<DebugLineGraphicsPipeline> pipeline{};
    DrawResultsGraphics lastFrameDrawResults{};
    bool enabled{false};
    float lineWidth{1.0};

public:
    // NOLINTBEGIN(readability-make-member-function-const): Manual propagation
    // of const-correctness

    void clear();
    void push(glm::vec3 start, glm::vec3 end);

    // NOLINTEND(readability-make-member-function-const)

    // Adds 4 line segmants defined by AB, BC, CD, DA.
    // Winding does not matter since these are added as separate line segments.
    void pushQuad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d);

    // Push a rectangle with possibly non-axis-aligned extents.
    void
    pushRectangleAxes(glm::vec3 center, glm::vec3 extentA, glm::vec3 extentB);

    // Push a rectangle, stretched along the (x,z) axes by extents.
    void pushRectangleOriented(
        glm::vec3 center, glm::quat orientation, glm::vec2 extents
    );

    // Push a rectangular prism, stretched along the (x,y,z) axes by extents.
    void pushBox(glm::vec3 center, glm::quat orientation, glm::vec3 extents);

    void recordCopy(VkCommandBuffer cmd) const;

    void cleanup(VkDevice device, VmaAllocator allocator);
};