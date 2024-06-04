#include "application.hpp"

#include "engine.hpp"

#include <iostream>

Application::Application()
{
    try
    {
        m_engine = Engine::loadEngine();
    }
    catch (std::exception const& exception)
    {
        std::cerr << "Failed to load engine:" << exception.what() << std::endl;
        // TODO: cleanup
        m_engine = nullptr;
    }
}

Application::~Application()
{
    delete m_engine;
}

void Application::run()
{
    try {
        Log("Running Engine.");
        m_engine->run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
