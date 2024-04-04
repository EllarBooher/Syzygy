#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <stb_image.h>

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

struct AllocatedImage {
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VkImage image{ VK_NULL_HANDLE };
    VkImageView imageView{ VK_NULL_HANDLE };
    VkExtent3D imageExtent{};
    VkFormat imageFormat{ VK_FORMAT_UNDEFINED };

    void cleanup(VkDevice device, VmaAllocator allocator)
    {
        vkDestroyImageView(device, imageView, nullptr);
        vmaDestroyImage(allocator, image, allocation);
    }
};

struct AllocatedBuffer {
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VmaAllocationInfo info{};
    VkBuffer buffer{ VK_NULL_HANDLE };

    void cleanup(VmaAllocator allocator)
    {
        vmaDestroyBuffer(allocator, buffer, allocation);
    }
};

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;

    void cleanup(VmaAllocator allocator)
    {
        indexBuffer.cleanup(allocator);
        vertexBuffer.cleanup(allocator);
    }
};

struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBufferAddress;
};

/** A quick and dirty way to keep track of the destruction order for vulkan objects. */
class DeletionQueue {
public:
    void pushFunction(std::function<void()>&& function) {
        cleanupCallbacks.push_front(function);
    }

    void flush() {
        for (std::function<void()> const& function : cleanupCallbacks)
        {
            function();
        }

        cleanupCallbacks.clear();
    }

private:
    std::deque<std::function<void()>> cleanupCallbacks{};

};