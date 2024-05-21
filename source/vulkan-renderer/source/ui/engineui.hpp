#pragma once

#include <span>
#include <memory>

#include <imgui.h>

#include "../pipelines.hpp"

struct MeshAsset;

struct UIRectangle
{
    glm::vec2 min{};
    glm::vec2 max{};

    glm::vec2 pos() const { return min; }
    glm::vec2 size() const { return max - min; }

    UIRectangle clampToMin() const
    {
        return UIRectangle{
            .min{ min },
            .max{ glm::max(min, max) },
        };
    }

    UIRectangle shrink(glm::vec2 const margins) const
    {
        return UIRectangle{
            .min{ min + margins },
            .max{ max - margins },
        };
    }
    UIRectangle shrinkMin(glm::vec2 const margins) const
    {
        return UIRectangle{
            .min{ min + margins },
            .max{ max },
        };
    }
    UIRectangle shrinkMax(glm::vec2 const margins) const
    {
        return UIRectangle{
            .min{ min },
            .max{ max - margins },
        };
    }
};

struct HUDState
{
    UIRectangle remainingArea{};

    std::optional<ImGuiID> leftDock{};
    std::optional<ImGuiID> rightDock{};
    std::optional<ImGuiID> bottomDock{};
};

HUDState renderHUD(glm::vec2 extent);

template<typename T>
void imguiStructureControls(T& structure, T const& defaultValues);

template<typename T>
void imguiStructureControls(T& structure);

template<typename T>
void imguiStructureDisplay(T const& structure);

void imguiMeshInstanceControls(
    bool& shouldRender,
    std::span<std::shared_ptr<MeshAsset> const> meshes,
    size_t& meshIndex
);

void imguiRenderingSelection(
    RenderingPipelines& currentActivePipeline
);

void imguiPerformanceWindow(
    std::span<double const> fpsValues, 
    double averageFPS, 
    size_t currentFrame,
    float& targetFPS
);

float draggableBar(
    std::string id
    , float initialPosition
    , bool horizontal // false = vertical
    , glm::vec2 min
    , glm::vec2 max
);