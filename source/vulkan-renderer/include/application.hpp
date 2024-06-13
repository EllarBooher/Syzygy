#pragma once

class Engine;

class Application
{
public:
    Application();
    ~Application();

    // Runs in a blocking manner.
    void run();

    bool loadedSuccessfully() const { return m_engine != nullptr; };

private:
    Engine* m_engine{nullptr};
};