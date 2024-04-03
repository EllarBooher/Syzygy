#include "application.hpp"

#include "engine.hpp"

#include <iostream>

Application::Application()
{
    m_engine = Engine::loadEngine();
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
