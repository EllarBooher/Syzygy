#include "engineui.hpp"

#include "syzygy/buffers.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/debuglines.hpp"
#include "syzygy/enginetypes.hpp"
#include "syzygy/pipelines.hpp"
#include "syzygy/shadowpass.hpp"
#include "syzygy/ui/propertytable.hpp"
#include <algorithm>
#include <array>
#include <fmt/core.h>
#include <imgui.h>
#include <memory>
#include <span>
#include <string>

void imguiRenderingSelection(RenderingPipelines& currentActivePipeline)
{
    auto const pipelineOrdering{std::to_array<RenderingPipelines>(
        {RenderingPipelines::DEFERRED, RenderingPipelines::COMPUTE_COLLECTION}
    )};
    auto const labels{
        std::to_array<std::string>({"Deferred", "Compute Collection"})
    };

    auto const selectedIt{std::find(
        pipelineOrdering.begin(), pipelineOrdering.end(), currentActivePipeline
    )};
    if (pipelineOrdering.end() == selectedIt)
    {
        // If we can't find what index this pipeline should be,
        // don't mess with it, since the engine may have set it.
        PropertyTable::begin()
            .rowReadOnlyText("", "Unknown pipeline selected")
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

    auto table{PropertyTable::begin()};

    table
        .rowReadOnlyText(
            "Pipeline",
            fmt::format(
                "0x{:x}", reinterpret_cast<uintptr_t>(structure.pipeline.get())
            )
        )
        .rowReadOnlyInteger(
            "Indices on GPU",
            static_cast<int32_t>(
                structure.indices != nullptr ? structure.indices->deviceSize()
                                             : 0
            )
        )
        .rowReadOnlyInteger(
            "Vertices on GPU",
            static_cast<int32_t>(
                structure.vertices != nullptr ? structure.vertices->deviceSize()
                                              : 0
            )
        );

    if (!structure.pipeline || !structure.indices || !structure.vertices)
    {
        table.rowReadOnlyBoolean("Enabled", structure.enabled);
    }
    else
    {
        table.rowBoolean("Enabled", structure.enabled, false);
    }

    table.rowFloat(
        "Line Width",
        structure.lineWidth,
        1.0F,
        PropertySliderBehavior{
            .bounds{0.0F, 100.0F},
        }
    );

    {
        DrawResultsGraphics const drawResults{structure.lastFrameDrawResults};

        table.rowChildPropertyBegin("Draw Results")
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
