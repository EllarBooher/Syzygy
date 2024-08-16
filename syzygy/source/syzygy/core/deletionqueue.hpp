#pragma once

#include "syzygy/core/log.hpp"
#include <deque>
#include <functional>

namespace syzygy
{
class DeletionQueue
{
public:
    void pushFunction(std::function<void()>&& function)
    {
        cleanupCallbacks.push_front(function);
    }

    void flush()
    {
        for (std::function<void()> const& function : cleanupCallbacks)
        {
            function();
        }

        cleanupCallbacks.clear();
    }
    void clear() { cleanupCallbacks.clear(); }

    ~DeletionQueue() noexcept
    {
        if (!cleanupCallbacks.empty())
        {
            SZG_WARNING("Cleanup callbacks was flushed.");
            flush();
        }
    }

private:
    std::deque<std::function<void()>> cleanupCallbacks{};
};
} // namespace syzygy