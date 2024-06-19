#include "../vulkanusage.hpp"
#include <optional>

template <typename T> struct VulkanResult
{
public:
    [[nodiscard]] auto value() const& -> T const&
    {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        return m_value.value();
    }

    [[nodiscard]] auto value() && -> T&&
    {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        return std::move(m_value).value();
    }

    [[nodiscard]] auto has_value() const -> bool { return m_value.has_value(); }

    [[nodiscard]] auto vk_result() const -> VkResult { return m_result; }

public:
    VulkanResult(T const& value, VkResult result)
        : m_value(value)
        , m_result(result)
    {
    }
    VulkanResult(T&& value, VkResult result)
        : m_value(std::move(value))
        , m_result(result)
    {
    }

    VulkanResult(VkResult result)
        : m_value(std::nullopt)
        , m_result(result)
    {
    }

    VulkanResult() = delete;

public:
    static auto make_value(T const& value, VkResult result) -> VulkanResult
    {
        return VulkanResult{value, result};
    }
    static auto make_value(T&& value, VkResult result) -> VulkanResult
    {
        return VulkanResult{std::move(value), result};
    }

    static auto make_empty(VkResult result) -> VulkanResult
    {
        return VulkanResult{result};
    }

private:
    std::optional<T> m_value;
    VkResult m_result;
};
