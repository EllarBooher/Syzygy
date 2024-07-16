#include "widgets.hpp"

#include "engineui.hpp"
#include <fmt/format.h>
#include <implot.h>

void ui::performanceWindow(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    RingBuffer const& values,
    float& targetFPS
)
{
    UIWindow const window{
        UIWindow::beginDockable(std::format("{}##performance", title), dockNode)
    };
    if (!window.open)
    {
        return;
    }

    ImGui::Text("%s", fmt::format("FPS: {:.1f}", values.average()).c_str());
    float const minFPS{10.0};
    float const maxFPS{1000.0};
    ImGui::DragScalar(
        "Target FPS",
        ImGuiDataType_Float,
        &targetFPS,
        1.0,
        &minFPS,
        &maxFPS,
        nullptr,
        ImGuiSliderFlags_AlwaysClamp
    );

    ImVec2 const plotSize{-1, 200};

    if (ImPlot::BeginPlot("FPS", plotSize))
    {
        ImPlot::SetupAxes(
            "",
            "FPS",
            ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock,
            ImPlotAxisFlags_LockMin
        );

        double constexpr DISPLAYED_FPS_MIN{0.0};
        double constexpr DISPLAYED_FPS_MAX{320.0};

        std::span<double const> const fpsValues{values.values()};

        ImPlot::SetupAxesLimits(
            0,
            static_cast<double>(fpsValues.size()),
            DISPLAYED_FPS_MIN,
            DISPLAYED_FPS_MAX
        );

        ImPlot::PlotLine(
            "##fpsValues",
            fpsValues.data(),
            static_cast<int32_t>(fpsValues.size())
        );

        size_t const currentIndex{values.current()};
        ImPlot::PlotInfLines("##current", &currentIndex, 1);

        ImPlot::EndPlot();
    }
}
