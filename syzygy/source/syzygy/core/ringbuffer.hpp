#pragma once

#include <numeric>
#include <span>
#include <vector>

namespace syzygy
{
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
} // namespace syzygy