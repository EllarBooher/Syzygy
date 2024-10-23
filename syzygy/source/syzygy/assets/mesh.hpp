#pragma once

#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/material.hpp"

namespace syzygy
{
struct GPUMeshBuffers
{
    GPUMeshBuffers() = delete;

    explicit GPUMeshBuffers(
        AllocatedBuffer&& indexBuffer, AllocatedBuffer&& vertexBuffer
    )
        : m_indexBuffer(std::move(indexBuffer))
        , m_vertexBuffer(std::move(vertexBuffer))
    {
    }

    GPUMeshBuffers(GPUMeshBuffers const& other) = delete;

    GPUMeshBuffers(GPUMeshBuffers&& other) = default;

    auto operator=(GPUMeshBuffers const& other) -> GPUMeshBuffers& = delete;

    auto operator=(GPUMeshBuffers&& other) -> GPUMeshBuffers& = default;

    // These are not const since they give access to the underlying memory.

    auto indexAddress() -> VkDeviceAddress
    {
        return m_indexBuffer.deviceAddress();
    }
    auto indexBuffer() -> VkBuffer { return m_indexBuffer.buffer(); }

    auto vertexAddress() -> VkDeviceAddress
    {
        return m_vertexBuffer.deviceAddress();
    }
    auto vertexBuffer() -> VkBuffer { return m_vertexBuffer.buffer(); }

private:
    AllocatedBuffer m_indexBuffer;
    AllocatedBuffer m_vertexBuffer;
};

// An interval of indices from an index buffer.
struct GeometrySurface
{
    uint32_t firstIndex;
    uint32_t indexCount;
    MaterialData material{};
};

struct Mesh
{
    std::vector<GeometrySurface> surfaces{};
    AABB vertexBounds{};
    std::unique_ptr<GPUMeshBuffers> meshBuffers{};
};
} // namespace syzygy