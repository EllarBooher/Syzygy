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

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/gtx/string_cast.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

struct UIPreferences
{
    float dpiScale{ 1.0f };
};

enum class RenderingPipelines
{
    DEFERRED = 0
    , COMPUTE_COLLECTION = 1
};

template<typename T>
struct TStagedBuffer;

struct MeshInstances
{
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> models{};
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> modelInverseTransposes{};

    std::vector<glm::mat4x4> originals{};

    // An index to where the first dynamic object begins
    size_t dynamicIndex{};
};

struct SceneBounds
{
    glm::vec3 center{};
    glm::vec3 extent{};
};

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct RingBuffer {
    void write(double const value)
    {
        m_values[m_index] = value;
        m_index += 1;
        m_saturated |= (m_index >= m_values.size());
        m_index = m_index % m_values.size();
    }

    static double arithmeticAverage(std::span<double const> const span)
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

// A quick and dirty way to keep track of the destruction order for 
// vulkan objects.
// TODO: deprecate this
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