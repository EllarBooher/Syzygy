#pragma once

#include <memory>

class Engine;

class Application
{
public:
    Application();

    // Destructor can't be inlined due to incomplete Engine type
    ~Application();

    // Runs in a blocking manner.
    void run();

    bool loadedSuccessfully() const { return m_engine.get() != nullptr; };

private:
    std::unique_ptr<Engine> m_engine{ nullptr };
};