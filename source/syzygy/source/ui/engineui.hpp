#pragma once

#include <memory>
#include <optional>
#include <span>

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

    static UIRectangle fromPosSize(glm::vec2 const pos, glm::vec2 const size)
    {
        return UIRectangle{
            .min{pos},
            .max{pos + size},
        };
    }

    UIRectangle clampToMin() const
    {
        return UIRectangle{
            .min{min},
            .max{glm::max(min, max)},
        };
    }

    UIRectangle shrink(glm::vec2 const margins) const
    {
        return UIRectangle{
            .min{min + margins},
            .max{max - margins},
        };
    }
    UIRectangle shrinkMin(glm::vec2 const margins) const
    {
        return UIRectangle{
            .min{min + margins},
            .max{max},
        };
    }
    UIRectangle shrinkMax(glm::vec2 const margins) const
    {
        return UIRectangle{
            .min{min},
            .max{max - margins},
        };
    }
};

// Opens the context for a Dear ImGui window, allowing further calls to the
// library to be placed within it.
struct UIWindow
{
    static auto
    beginMaximized(std::string const& name, UIRectangle const workArea)
        -> UIWindow;

    static auto beginDockable(
        std::string const& name, std::optional<ImGuiID> const dockspace
    ) -> UIWindow;

    UIWindow& operator=(UIWindow const& other) = delete;
    UIWindow& operator=(UIWindow&& other) = delete;
    UIWindow(UIWindow&& other) noexcept;

    ~UIWindow();

    UIRectangle screenRectangle{};
    bool open{false};

private:
    UIWindow(UIRectangle screenRectangle, bool open, uint16_t styleVariables)
        : screenRectangle{screenRectangle}
        , open{open}
        , m_styleVariables{styleVariables}
        , m_initialized{true}
    {
    }

    uint16_t m_styleVariables{0};
    bool m_initialized{false};
};

struct HUDState
{
    UIRectangle workArea{};

    // The background window that acts as the parent of all the laid out windows
    ImGuiID dockspaceID{};

    bool maximizeSceneViewport{false};

    bool rebuildLayoutRequested{false};

    bool resetPreferencesRequested{false};
    bool applyPreferencesRequested{false};
};

HUDState renderHUD(UIPreferences& preferences);

struct DockingLayout
{
    std::optional<ImGuiID> left{};
    std::optional<ImGuiID> right{};
    std::optional<ImGuiID> centerBottom{};
    std::optional<ImGuiID> centerTop{};
};

namespace ui
{
struct RenderTarget
{
    glm::vec2 extent;
};

auto sceneViewport(
    VkDescriptorSet sceneTexture,
    VkExtent2D sceneTextureExtent,
    std::optional<UIRectangle> maximizedArea,
    std::optional<ImGuiID> dockspace
) -> std::optional<RenderTarget>;
} // namespace ui

// Builds a hardcoded hierarchy of docking nodes from the passed parent.
// This also may break layouts, if windows have been moved or docked,
// since all new IDs are generated.
DockingLayout
buildDefaultMultiWindowLayout(UIRectangle workArea, ImGuiID parentNode);

template <typename T>
void imguiStructureControls(T& structure, T const& defaultStructure);

template <typename T> void imguiStructureControls(T& structure);

template <typename T> void imguiStructureDisplay(T const& structure);

void imguiMeshInstanceControls(
    bool& shouldRender,
    std::span<std::shared_ptr<MeshAsset> const> meshes,
    size_t& meshIndex
);

void imguiRenderingSelection(RenderingPipelines& currentActivePipeline);

struct PerformanceValues
{
    std::span<double const> samplesFPS;
    double averageFPS;

    // Used to draw a vertical line indicating the current frame
    size_t currentFrame;
};

void imguiPerformanceDisplay(PerformanceValues values, float& targetFPS);