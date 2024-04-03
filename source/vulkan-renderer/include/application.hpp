#pragma once

#include <memory>

class Engine;

class Application
{
public:
    Application();

    // Destructor can't be inlined due to incomplete Engine type
    ~Application();

    /** Runs in a blocking manner. Returns control when execution ends. */
    void run();

private:
    std::unique_ptr<Engine> m_engine{ nullptr };
};