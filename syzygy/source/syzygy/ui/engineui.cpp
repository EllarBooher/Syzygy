#include "engineui.hpp"

#include "syzygy/platform/integer.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include "syzygy/renderer/pipelines/debuglines.hpp"
#include "syzygy/renderer/shadowpass.hpp"
#include "syzygy/ui/propertytable.hpp"
#include <algorithm>
#include <array>
#include <imgui.h>
#include <memory>
#include <span>
#include <spdlog/fmt/bundled/core.h>
#include <string>

namespace syzygy
{
void imguiRenderingSelection(RenderingPipelines& currentActivePipeline)
{
    auto const pipelineOrdering{std::to_array<RenderingPipelines>(
        {RenderingPipelines::DEFERRED, RenderingPipelines::COMPUTE_COLLECTION}
    )};
    auto const labels{
        std::to_array<std::string>({"Deferred", "Compute Collection"})
    };
    static_assert(labels.size() == pipelineOrdering.size());

    auto const selectedIt{std::find(
        pipelineOrdering.begin(), pipelineOrdering.end(), currentActivePipeline
    )};
    if (pipelineOrdering.end() == selectedIt)
    {
        // If we can't find what index this pipeline should be,
        // don't mess with it, since the engine may have set it.
        PropertyTable::begin()
            .rowTextLabel("", "Unknown pipeline selected")
            .end();
    }
    else
    {
        size_t const defaultIndex{0};

        auto selectedIndex{static_cast<size_t>(
            std::distance(pipelineOrdering.begin(), selectedIt)
        )};

        PropertyTable::begin()
            .rowDropdown(
                "Rendering Pipeline", selectedIndex, defaultIndex, labels
            )
            .end();

        currentActivePipeline = pipelineOrdering[selectedIndex];
    }
}

template <> void imguiStructureControls<DebugLines>(DebugLines& structure)
{
    bool const headerOpen{
        ImGui::CollapsingHeader("Debug Lines", ImGuiTreeNodeFlags_DefaultOpen)
    };

    if (!headerOpen)
    {
        return;
    }

    std::array<
        std::tuple<syzygy::DebugLinesLayers, char const*>,
        2> constexpr LAYER_LABELS{
        {{DebugLinesLayers::Gizmo, "Gizmo"}, {DebugLinesLayers::Debug, "Debug"}}
    };

    auto table{PropertyTable::begin()};

    table.rowChildPropertyBegin("Layers", false);
    for (auto const& [layer, label] : LAYER_LABELS)
    {
        bool enabled{structure.getLayerEnabled(layer)};
        table.rowBoolean(label, enabled, enabled);
        structure.setLayerEnabled(layer, enabled);
    }
    table.childPropertyEnd();

    {
        DebugLinesRenderInfo const renderInfo{structure.renderInfo()};

        table.rowChildPropertyBegin("Render Info", false)
            .rowReadOnlyInteger(
                "Indices on GPU", static_cast<int32_t>(renderInfo.indicesOnGPU)
            )
            .rowReadOnlyInteger(
                "Vertices on GPU",
                static_cast<int32_t>(renderInfo.verticesOnGPU)
            )
            .childPropertyEnd();
    }

    {
        DrawResultsGraphics const drawResults{structure.lastFrameDrawResults()};

        table.rowChildPropertyBegin("Draw Results", false)
            .rowReadOnlyInteger(
                "Draw Calls", static_cast<int32_t>(drawResults.drawCalls)
            )
            .rowReadOnlyInteger(
                "Vertices Drawn",
                static_cast<int32_t>(drawResults.verticesDrawn)
            )
            .rowReadOnlyInteger(
                "Indices Drawn", static_cast<int32_t>(drawResults.indicesDrawn)
            )
            .childPropertyEnd();
    }

    table.end();
}

template <>
void imguiStructureControls(
    ShadowPassParameters& structure,
    ShadowPassParameters const& defaultStructure
)
{
    bool const headerOpen{ImGui::CollapsingHeader(
        "Shadow Pass Parameters", ImGuiTreeNodeFlags_DefaultOpen
    )};

    if (!headerOpen)
    {
        return;
    }

    float constexpr DEPTH_BIAS_SPEED{0.01F};

    PropertyTable::begin()
        .rowFloat(
            "Depth Bias Constant",
            structure.depthBiasConstant,
            defaultStructure.depthBiasConstant,
            PropertySliderBehavior{
                .speed = DEPTH_BIAS_SPEED,
            }
        )
        .rowReadOnlyBoolean("Depth Bias Clamp", false)
        .rowFloat(
            "Depth Bias Slope",
            structure.depthBiasSlope,
            defaultStructure.depthBiasSlope,
            PropertySliderBehavior{
                .speed = DEPTH_BIAS_SPEED,
            }
        )
        .end();
}
} // namespace syzygy