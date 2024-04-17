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
        std::cerr << exception.what() << std::endl;
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
