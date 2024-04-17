#pragma once

#include <span>

template<typename T>
void imguiStructureControls(T& structure, T const& defaultValues);

template<typename T>
void imguiStructureControls(T& structure);

void imguiPerformanceWindow(
    std::span<double const> fpsValues, 
    double averageFPS, 
    size_t currentFrame,
    float& targetFPS);