#pragma once

#include "syzygy/helpers.hpp"
#include <deque>
#include <functional>

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