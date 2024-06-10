#pragma once

#include <span>
#include <memory>

#include <imgui.h>

#include "../enginetypes.hpp"
#include "../pipelines.hpp"

struct MeshAsset;

struct UIRectangle
{
    glm::vec2 min{};
    glm::vec2 max{};

    glm::vec2 pos() const { return min; }
    glm::vec2 size() const { return max - min; }

    static UIRectangle fromPosSize(
        glm::vec2 const pos
        , glm::vec2 const size
    )
    {
        return UIRectangle{
            .min{ pos },
            .max{ pos + size },
        };
    }

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
    UIRectangle workArea{};
    UIRectangle sceneViewport{};

    // The background window that acts as the parent of all the laid out windows
    ImGuiID dockspaceID{};

    bool maximizeSceneViewport{ false };

    bool resetLayoutRequested{ false };

    bool resetPreferencesRequested{ false };
    bool applyPreferencesRequested{ false };
};

HUDState renderHUD(UIPreferences& preferences);

struct DockingLayout
{
    ImGuiID left{};
    ImGuiID right{};
    ImGuiID centerBottom{};
    ImGuiID centerTop{};
};

// Builds a hardcoded hierarchy of docking nodes from the passed parent.
// This also may break layouts, if windows have been moved or docked, 
// since all new IDs are generated.
DockingLayout buildDefaultMultiWindowLayout(
    UIRectangle workArea
    , ImGuiID parentNode
);

template<typename T>
void imguiStructureControls(T& structure, T const& defaultStructure);

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

struct PerformanceValues
{
    std::span<double const> samplesFPS;
    double averageFPS;

    // Used to draw a vertical line indicating the current frame
    size_t currentFrame;
};

void imguiPerformanceWindow(
    PerformanceValues values
    , float& targetFPS
);