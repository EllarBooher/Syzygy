#include "pipelineui.hpp"

#include "syzygy/platform/integer.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include "syzygy/renderer/pipelines/deferred.hpp"
#include "syzygy/renderer/shaders.hpp"
#include "syzygy/renderer/shadowpass.hpp"
#include "syzygy/ui/engineui.hpp"
#include "syzygy/ui/propertytable.hpp"
#include <cassert>
#include <imgui.h>
#include <limits>
#include <span>
#include <spdlog/fmt/bundled/core.h>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
void TypeLabel(std::string const& label)
{
    ImVec2 const textSize{ImGui::CalcTextSize(label.c_str(), nullptr, true)};

    float const buttonWidth{textSize.x + 10.0F};

    ImGui::SameLine(ImGui::GetWindowWidth() - buttonWidth, 0.0);
    ImGui::Text("%s", label.c_str());
}
} // namespace

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

namespace syzygy
{
// NOLINTBEGIN
// TODO: refactor this awul, no good, function.
template <typename T>
static void imguiPushStructureControl(
    ShaderReflectionData::PushConstant const& pushConstant,
    bool const readOnly,
    std::span<T> const backingData
)
{
    bool const headerOpen{ImGui::CollapsingHeader(
        pushConstant.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen
    )};

    if (!headerOpen)
    {
        return;
    }

    static_assert(
        std::is_same<T, uint8_t>::value || std::is_same<T, uint8_t const>::value
    );

    ShaderReflectionData::Structure const& structure{pushConstant.type};

    ImGui::Text("%s (%s)", "Push Constant", readOnly ? "Read Only" : "Mutable");
    {
        ImGui::BeginTable(
            "Push Constant Reflection Data",
            2,
            ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH
                | ImGuiTableFlags_RowBg
        );

        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        int32_t const columnIndexProperty{0};
        int32_t const columnIndexValue{1};

        ImGui::TableHeadersRow();
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(columnIndexProperty);
            ImGui::Text("Layout Byte Offset");
            ImGui::TableSetColumnIndex(columnIndexValue);
            ImGui::Text(
                "%s", fmt::format("{}", pushConstant.layoutOffsetBytes).c_str()
            );
        }
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(columnIndexProperty);
            ImGui::Text("Byte Size");
            ImGui::TableSetColumnIndex(columnIndexValue);
            ImGui::Text("%s", fmt::format("{}", structure.sizeBytes).c_str());
        }

        ImGui::EndTable();
    }

    {
        ImGui::BeginTable(
            "Push Constant Table",
            6,
            ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter
                | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg
        );

        ImGui::TableSetupColumn(
            "Member Name", ImGuiTableColumnFlags_WidthStretch
        );
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Padded", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        int32_t const memberNameIndex{0};
        int32_t const columnIndexValue{1};
        int32_t const typeIndex{2};
        int32_t const offsetIndex{3};
        int32_t const columnIndexSize{4};
        int32_t const columnIndexPaddedSize{5};

        // Members can be sparse, with implied padding between them.
        // We track those bytes so we can print to the UI that there is padding.
        uint32_t lastByte{0};
        for (ShaderReflectionData::Member const& member : structure.members)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(offsetIndex);
            ImGui::Text("%s", fmt::format("{}", member.offsetBytes).c_str());

            ImGui::TableSetColumnIndex(memberNameIndex);
            ImGui::Text("%s", member.name.c_str());

            ImGui::TableSetColumnIndex(columnIndexSize);
            ImGui::Text("%s", fmt::format("{}", member.type.sizeBytes).c_str());

            ImGui::TableSetColumnIndex(columnIndexPaddedSize);
            ImGui::Text(
                "%s", fmt::format("{}", member.type.paddedSizeBytes).c_str()
            );

            std::visit(
                overloaded{
                    [&](ShaderReflectionData::UnsupportedType const&
                            unsupportedType)
            {
                ImGui::Text(
                    "%s",
                    fmt::format("Unsupported member \"{}\"", member.name)
                        .c_str()
                );
            },
                    [&](ShaderReflectionData::Pointer const& pointerType)
            {
                // Pointers should be 8 bytes,
                // I am not sure when this would fail
                assert(member.type.sizeBytes == 8);

                std::string const memberLabel{fmt::format("##{}", member.name)};

                size_t byteOffset{member.offsetBytes};

                if (structure.paddedSizeBytes > backingData.size())
                {
                    // Hacky fix for when backingdata is offset
                    // TODO: Make runtime push constant data consistant
                    // with offsets
                    byteOffset -= pushConstant.layoutOffsetBytes;
                }

                T* const pDataPointer{&backingData[byteOffset]};

                ImGui::TableSetColumnIndex(columnIndexValue);

                ImGui::BeginDisabled(readOnly);
                {
                    ImGui::PushItemWidth(-std::numeric_limits<float>::min()
                    ); // This hides the label
                    ImGui::InputScalar(
                        memberLabel.c_str(),
                        ImGuiDataType_U64,
                        reinterpret_cast<void*>(
                            const_cast<uint8_t*>(pDataPointer)
                        )
                    );
                    ImGui::PopItemWidth();
                }
                ImGui::EndDisabled();

                ImGui::TableSetColumnIndex(typeIndex);
                ImGui::Text("Pointer");
            },
                    [&](ShaderReflectionData::NumericType const& numericType)
            {
                // Gather format data
                uint32_t columns{0};
                uint32_t rows{0};
                std::visit(
                    overloaded{
                        [&](ShaderReflectionData::Scalar const& scalar)
                {
                    columns = 1;
                    rows = 1;
                },
                        [&](ShaderReflectionData::Vector const& vector)
                {
                    columns = 1;
                    rows = vector.componentCount;
                },
                        [&](ShaderReflectionData::Matrix const& matrix)
                {
                    columns = matrix.columnCount;
                    rows = matrix.rowCount;
                },
                    },
                    numericType.format
                );

                bool bSupportedType{true};
                ImGuiDataType imguiDataType{ImGuiDataType_Float};
                std::visit(
                    overloaded{
                        [&](ShaderReflectionData::Integer const&
                                integerComponent)
                {
                    assert(
                        integerComponent.signedness == 0
                        || integerComponent.signedness == 1
                    );

                    if (integerComponent.signedness)
                    {
                        switch (numericType.componentBitWidth)
                        {
                        case 8:
                            imguiDataType = ImGuiDataType_U8;
                            break;
                        case 16:
                            imguiDataType = ImGuiDataType_U16;
                            break;
                        case 32:
                            imguiDataType = ImGuiDataType_U32;
                            break;
                        case 64:
                            imguiDataType = ImGuiDataType_U64;
                            break;
                        default:
                            bSupportedType = false;
                            break;
                        }
                    }
                    else
                    {
                        switch (numericType.componentBitWidth)
                        {
                        case 8:
                            imguiDataType = ImGuiDataType_S8;
                            break;
                        case 16:
                            imguiDataType = ImGuiDataType_S16;
                            break;
                        case 32:
                            imguiDataType = ImGuiDataType_S32;
                            break;
                        case 64:
                            imguiDataType = ImGuiDataType_S64;
                            break;
                        default:
                            bSupportedType = false;
                            break;
                        }
                    }
                },
                        [&](ShaderReflectionData::Float const& floatComponent)
                {
                    ImGuiDataType imguiDataType{ImGuiDataType_Float};
                    switch (numericType.componentBitWidth)
                    {
                    case 64:
                        imguiDataType = ImGuiDataType_Double;
                        break;
                    case 32:
                        imguiDataType = ImGuiDataType_Float;
                        break;
                    default:
                        bSupportedType = false;
                        break;
                    }
                },
                    },
                    numericType.componentType
                );

                ImGui::TableSetColumnIndex(typeIndex);
                ImGui::Text("Numeric Type");

                if (!bSupportedType)
                {
                    ImGui::TableSetColumnIndex(columnIndexValue);
                    ImGui::Text(
                        "%s",
                        fmt::format(
                            "Unsupported component bit width {} "
                            "for member {}",
                            numericType.componentBitWidth,
                            member.name
                        )
                            .c_str()
                    );
                    return;
                }

                // SPIR-V aggregate types are column major.
                // We render each "column" of the spirv data type as a
                // "row" of imgui inputs to avoid flipping.
                for (uint32_t column{0}; column < columns; column++)
                {
                    size_t byteOffset{
                        (column * rows * numericType.componentBitWidth / 8)
                        + member.offsetBytes
                    };
                    if (structure.paddedSizeBytes > backingData.size())
                    {
                        // Hacky fix for when backingdata is offset
                        // TODO: Make runtime push constant data
                        // consistant with offsets
                        byteOffset -= pushConstant.layoutOffsetBytes;
                    }

                    T* const pData{&backingData[byteOffset]};

                    // Check that ImGui won't modify out of bounds data
                    size_t const lastByte{
                        static_cast<size_t>(
                            rows * numericType.componentBitWidth / 8
                        )
                        + byteOffset
                    };
                    assert(lastByte <= backingData.size());

                    std::string const rowLabel{
                        fmt::format("##{}{}", member.name, column)
                    };

                    ImGui::TableSetColumnIndex(columnIndexValue);
                    ImGui::BeginDisabled(readOnly);
                    {
                        // This hides the label
                        ImGui::PushItemWidth(-std::numeric_limits<float>::min()
                        );
                        ImGui::InputScalarN(
                            rowLabel.c_str(),
                            imguiDataType,
                            reinterpret_cast<void*>(const_cast<uint8_t*>(pData)
                            ),
                            static_cast<int32_t>(rows),
                            nullptr,
                            nullptr,
                            imguiDataType == ImGuiDataType_Float ? "%.6f"
                                                                 : nullptr
                        );
                        ImGui::PopItemWidth();
                    }
                    ImGui::EndDisabled();
                }
            },
                },
                member.type.typeData
            );
        }

        ImGui::EndTable();
    }
}
// NOLINTEND

template <> void imguiPipelineControls(ComputeCollectionPipeline& pipeline)
{
    if (!ImGui::CollapsingHeader(
            "Compute Collection Pipeline", ImGuiTreeNodeFlags_DefaultOpen
        ))
    {
        return;
    }

    std::vector<std::string> shaderNames{};
    for (ShaderObjectReflected const& shader : pipeline.shaders())
    {
        shaderNames.push_back(shader.name());
    }

    size_t currentShaderIndex{pipeline.shaderIndex()};

    PropertyTable table{PropertyTable::begin().rowDropdown(
        "Active Shader", currentShaderIndex, 0, shaderNames
    )};

    pipeline.selectShader(currentShaderIndex);

    ShaderObjectReflected const& currentShader{pipeline.currentShader()};
    ShaderReflectionData const& reflectionData{currentShader.reflectionData()};

    if (reflectionData.defaultEntryPointHasPushConstant())
    {
        table.end();
        imguiPushStructureControl(
            reflectionData.defaultPushConstant(),
            false,
            pipeline.mapPushConstantBytes()
        );
    }
    else
    {
        table.rowTextLabel("", "No push constants.");
        table.end();
    }
}

template <> void imguiPipelineControls(DeferredShadingPipeline& pipeline)
{
    DeferredShadingPipeline::Configuration config{pipeline.getConfiguration()};
    imguiStructureControls(config.shadowPassParameters, ShadowPassParameters{});
    pipeline.setConfiguration(config);
}
} // namespace syzygy