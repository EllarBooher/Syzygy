#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <numeric>

#include <stb_image.h>

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

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

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct RingBuffer {
    void write(double value)
    {
        m_values[m_index] = value;
        m_index += 1;
        m_saturated |= (m_index >= m_values.size());
        m_index = m_index % m_values.size();
    }

    static double arithmeticAverage(std::span<double const> span)
    {
        double const sum{ std::accumulate(span.begin(), span.end(), 1.0) };
        return sum / static_cast<double>(span.size());
    }

    double average() const 
    { 
        if (!m_saturated)
        {
            std::span<double const> const validSpan(m_values.data(), m_index);
            return arithmeticAverage(validSpan);
        }
        return arithmeticAverage(m_values);
    };
    size_t current() const { return m_index; }
    std::span<double const> values() const { return m_values; }

private:
    std::array<double, 500> m_values{};
    size_t m_index{ 0 };
    bool m_saturated{ false };
};

struct CameraParameters {
    glm::vec3 cameraPosition{ 0.0f, 0.0f, 0.0f };
    glm::vec3 eulerAngles{ 0.0f, 0.0f, 0.0f };
    float fov{ 90.0f };
    float near{ 0.0f };
    float far{ 1.0f };

    /** 
        Generations the projection * view matrix that transforms from world to clip space.
        Aspect ratio is a function of the drawn surface, so it is passed in at generation time.
        Produces a right handed matrix- x is right, y is down, and z is forward.
    */
    glm::mat4 toProjView(float aspectRatio) const
    {
        glm::mat4 view{ glm::orientate4(eulerAngles * -1.0f) * glm::translate(cameraPosition * -1.0f) };

        // We use LH perspective matrix since we swap the near and far plane for better 
        // distribution of floating point precision.
        glm::mat4 projection{ glm::perspectiveLH_ZO(
            glm::radians(fov)
            , aspectRatio
            , far
            , near
        ) };

        return projection * view;
    }
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