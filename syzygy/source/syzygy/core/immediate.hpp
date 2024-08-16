#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include <functional>
#include <optional>
#include <utility>

namespace syzygy
{
struct ImmediateSubmissionQueue
{
public:
    ImmediateSubmissionQueue() noexcept = default;
    ImmediateSubmissionQueue(ImmediateSubmissionQueue&& other) noexcept
    {
        *this = std::move(other);
    }

    ImmediateSubmissionQueue& operator=(ImmediateSubmissionQueue&& other
    ) noexcept;

    ~ImmediateSubmissionQueue() noexcept { destroy(); }

    static auto create(VkDevice, uint32_t queueFamilyIndex)
        -> std::optional<ImmediateSubmissionQueue>;

    enum class SubmissionResult
    {
        NOT_INITIALZIED,
        FAILED,
        TIMEOUT,
        SUCCESS
    };

    // The passed queue should support the desired operations that will be
    // recorded in the callback. This waits for the submission to complete.
    auto immediateSubmit(
        VkQueue const queue,
        std::function<void(VkCommandBuffer cmd)>&& recordingCallback
    ) const -> SubmissionResult;

private:
    void destroy();

    ImmediateSubmissionQueue(
        VkDevice device,
        VkFence busyFence,
        VkCommandPool pool,
        VkCommandBuffer cmd
    )
        : m_device{device}
        , m_busyFence{busyFence}
        , m_commandPool{pool}
        , m_commandBuffer{cmd}
    {
    }

    VkDevice m_device{VK_NULL_HANDLE};
    VkFence m_busyFence{VK_NULL_HANDLE};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
};
} // namespace syzygy