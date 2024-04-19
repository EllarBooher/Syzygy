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
        m_engine.reset();
    }
}

Application::~Application()
{

}

void Application::run()
{
    try {
        m_engine->run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
