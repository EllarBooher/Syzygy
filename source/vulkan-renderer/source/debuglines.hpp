#pragma once

#include "enginetypes.hpp"

struct DebugLines
{
    std::unique_ptr<DebugLineComputePipeline> pipeline{};

    std::unique_ptr<TStagedBuffer<Vertex>> vertices{};
    std::unique_ptr<TStagedBuffer<uint32_t>> indices{};

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
    void recordCopy(VkCommandBuffer cmd, VmaAllocator allocator)
    {
        vertices->recordCopyToDevice(cmd, allocator);
        indices->recordCopyToDevice(cmd, allocator);
    }
    void cleanup(VkDevice device, VmaAllocator allocator)
    {
        pipeline->cleanup(device);
        vertices.reset();
        indices.reset();
    }
};