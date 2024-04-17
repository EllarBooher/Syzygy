#include "engineui.hpp"

#include <imgui.h>
#include <implot.h>
#include <fmt/format.h>

#include "../shaders.hpp"
#include "../engineparams.hpp"

void imguiPerformanceWindow(
    std::span<double const> fpsValues
    , double averageFPS
    , size_t currentFrame
    , float& targetFPS)
{
    if (ImGui::Begin("Performance Information"))
    {
        ImGui::Text(fmt::format("FPS: {:.1f}", averageFPS).c_str());
        float const minFPS{ 10.0 };
        float const maxFPS{ 1000.0 };
        ImGui::DragScalar("Target FPS", ImGuiDataType_Float, &targetFPS, 1.0, &minFPS, &maxFPS, nullptr, ImGuiSliderFlags_AlwaysClamp);
        if (ImPlot::BeginPlot("FPS"))
        {
            ImPlot::SetupAxes("", "FPS", ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock, ImPlotAxisFlags_LockMin);
            ImPlot::SetupAxesLimits(0, fpsValues.size(), 0.0f, 320.0f);
            ImPlot::PlotLine("Test", fpsValues.data(), fpsValues.size());
            ImPlot::PlotInfLines("Current Frame", &currentFrame, 1);
            ImPlot::EndPlot();
        }
    }
    ImGui::End();
}

template<>
void imguiStructureControls<CameraParameters>(CameraParameters& structure)
{
    ImGui::BeginGroup();
    ImGui::Text("Camera Parameters");
    ImGui::DragScalarN("cameraPosition", ImGuiDataType_Float, &structure.cameraPosition, 3, 0.2f);
    ImGui::DragScalarN("eulerAngles", ImGuiDataType_Float, &structure.eulerAngles, 3, 0.1f);

    float const fovMin{ 0.0f };
    float const fovMax{ 180.0f };
    ImGui::DragScalarN("fov", ImGuiDataType_Float, &structure.fov, 1, 1.0f, &fovMin, &fovMax);

    float const nearPlaneMin{ 0.01f };
    float const nearPlaneMax{ structure.far - 0.01f };
    ImGui::DragScalarN("nearPlane", ImGuiDataType_Float, &structure.near, 1, structure.near * 0.01f, &nearPlaneMin, &nearPlaneMax);

    float const farPlaneMin{ structure.near + 0.01f };
    float const farPlaneMax{ 1'000'000.0f };
    ImGui::DragScalarN("farPlane", ImGuiDataType_Float, &structure.far, 1, structure.far * 0.01f, &farPlaneMin, &farPlaneMax);

    ImGui::EndGroup();
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
void imguiPushStructureControl(
    ShaderReflectionData::Structure const& structure,
    bool readOnly,
    std::span<uint8_t> backingData
)
{
    for (ShaderReflectionData::StructureMember const& member : structure.members)
    {
        std::visit(overloaded{
            [&](ShaderReflectionData::UnsupportedType const& unsupportedType) {
                ImGui::Text(fmt::format("Unsupported member \"{}\"", member.name).c_str());
            },
            [&](ShaderReflectionData::NumericType const& numericType) {

                // Gather format data
                uint32_t columns{ 0 };
                uint32_t rows{ 0 };
                std::visit(overloaded{
                    [&](ShaderReflectionData::Scalar const& scalar) {
                        columns = 1;
                        rows = 1;
                    },
                    [&](ShaderReflectionData::Vector const& vector) {
                        columns = 1;
                        rows = vector.componentCount;
                    },
                    [&](ShaderReflectionData::Matrix const& matrix) {
                        columns = matrix.columnCount;
                        rows = matrix.rowCount;
                    },
                }, numericType.format);

                bool bSupportedType{ true };
                ImGuiDataType imguiDataType{ ImGuiDataType_Float };
                std::visit(overloaded{
                    [&](ShaderReflectionData::Integer const& integerComponent) {
                        assert(integerComponent.signedness == 0 || integerComponent.signedness == 1);

                        switch (integerComponent.signedness)
                        {
                        case 0: //unsigned
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
                            break;
                        default: //signed
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
                            break;
                        }
                    },
                    [&](ShaderReflectionData::Float const& floatComponent) {
                        bool bSupportedFloat{ true };
                        ImGuiDataType imguiDataType{ ImGuiDataType_Float };
                        switch (numericType.componentBitWidth)
                        {
                        case 64:
                            imguiDataType = ImGuiDataType_Double;
                            break;
                        case 32:
                            imguiDataType = ImGuiDataType_Float;
                            break;
                        default:
                            bSupportedFloat = false;
                            break;
                        }
                    },
                }, numericType.componentType);

                if (!bSupportedType)
                {
                    ImGui::Text(fmt::format("Unsupported component bit width {} for member {}", numericType.componentBitWidth, member.name).c_str());
                }

                // SPIR-V aggregate types are column major. 
                // Columns are members, and rows are components of a vector/column.
                for (uint32_t column{ 0 }; column < columns; column++)
                {
                    size_t const byteOffset{ column * rows * numericType.componentBitWidth / 8 + member.offsetBytes };
                    uint8_t* const pDataPointer{ &backingData[byteOffset] };

                    // Check that ImGui won't modify out of bounds data
                    assert((byteOffset + rows * numericType.componentBitWidth / 8) <= backingData.size());

                    std::string const rowLabel{ fmt::format("{}##{}", member.name, column) };

                    ImGui::BeginDisabled(readOnly);
                    ImGui::InputScalarN(
                        rowLabel.c_str(),
                        imguiDataType,
                        reinterpret_cast<void*>(pDataPointer),
                        rows
                    );
                    ImGui::EndDisabled();
                }
            },
            }, member.type.typeData);
    }
}

template<>
void imguiStructureControls<ShaderWrapper>(ShaderWrapper& shader)
{
    if (ImGui::CollapsingHeader(fmt::format("{} controls", shader.name()).c_str()))
    {
        ImGui::Indent(16.0f);
        if (shader.reflectionData().pushConstantsByEntryPoint.empty())
        {
            ImGui::Text("No push constants.");
        }
        for (auto const& [entryPoint, pushConstant] : shader.reflectionData().pushConstantsByEntryPoint)
        {
            if (ImGui::CollapsingHeader(fmt::format("\"{}\", entry point \"{}\"", pushConstant.name, entryPoint).c_str()))
            {
                std::span<uint8_t> const pushConstantMappedData{ shader.mapRuntimePushConstant(entryPoint) };

                imguiPushStructureControl(pushConstant.type, false, pushConstantMappedData);
            }
        }
        ImGui::Unindent(16.0f);
    }
}

template<>
void imguiStructureControls<AtmosphereParameters>(AtmosphereParameters& atmosphere)
{
    ImGui::BeginGroup();

    ImGui::Text("Atmosphere Parameters");
    ImGui::DragScalarN("directionToSun", ImGuiDataType_Float, &atmosphere.directionToSun, 3, 0.1f);

    ImGui::DragScalar("earthRadiusMeters", ImGuiDataType_Float, &atmosphere.earthRadiusMeters, 0.1f);
    ImGui::DragScalar("atmosphereRadiusMeters", ImGuiDataType_Float, &atmosphere.atmosphereRadiusMeters, 0.1f);

    ImGui::DragScalarN("scatteringCoefficientRayleigh", ImGuiDataType_Float, &atmosphere.scatteringCoefficientRayleigh, 3, 0.1f);
    ImGui::DragScalar("altitudeDecayRayleigh", ImGuiDataType_Float, &atmosphere.altitudeDecayRayleigh, 0.1f);

    ImGui::DragScalarN("scatteringCoefficientMie", ImGuiDataType_Float, &atmosphere.scatteringCoefficientMie, 3, 0.1f);
    ImGui::DragScalar("altitudeDecayMie", ImGuiDataType_Float, &atmosphere.altitudeDecayMie, 0.1f);

    ImGui::EndGroup();
}