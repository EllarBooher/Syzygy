#include "application.hpp"

#include "engine.hpp"

#include <iostream>

Application::Application() { m_engine = Engine::loadEngine(); }

Application::~Application() { delete m_engine; }

void Application::run()
{
    try
    {
        Log("Running Engine.");
        m_engine->run();
    }
    catch (std::exception const& e)
    {
        std::cerr << e.what() << '\n';
    }
}
