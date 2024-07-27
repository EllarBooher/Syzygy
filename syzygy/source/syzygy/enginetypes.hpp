#pragma once

#include <array>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "vulkanusage.hpp"
#include <vulkan/vk_enum_string_helper.h>

#include <fmt/core.h>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

struct UIPreferences
{
    float dpiScale{1.0f};
};

enum class RenderingPipelines
{
    DEFERRED = 0,
    COMPUTE_COLLECTION = 1
};

template <typename T> struct TStagedBuffer;

struct MeshInstances
{
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> models{};
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> modelInverseTransposes{};

    std::vector<glm::mat4x4> originals{};

    // An index to where the first dynamic object begins
    size_t dynamicIndex{};
};

struct Vertex
{
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct RingBuffer
{
    RingBuffer() { m_values.resize(500, 0.0); }

    void write(double const value)
    {
        m_values[m_index] = value;
        m_index += 1;
        m_saturated |= (m_index >= m_values.size());
        m_index = m_index % m_values.size();
    }

    static double arithmeticAverage(std::span<double const> const span)
    {
        double const sum{std::accumulate(span.begin(), span.end(), 1.0)};
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
    std::vector<double> m_values{};
    size_t m_index{0};
    bool m_saturated{false};
};