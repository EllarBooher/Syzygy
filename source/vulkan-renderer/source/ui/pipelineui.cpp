#include "pipelineui.hpp"
#include "engineui.hpp"

#include <imgui.h>
#include <fmt/format.h>

#include "../pipelines.hpp"

static void TypeLabel(std::string const& label)
{
    ImVec2 const textSize{ ImGui::CalcTextSize(label.c_str(), nullptr, true) };

    float const buttonWidth{ textSize.x + 10.0f};

    ImGui::SameLine(ImGui::GetWindowWidth() - buttonWidth, 0.0);
    ImGui::Text(label.c_str());
}

template<class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

template<typename T>
static void imguiPushStructureControl(
    ShaderReflectionData::PushConstant const& pushConstant,
    bool readOnly,
    std::span<T> backingData
)
{
    // This whole method is bad and should be refactored in a way to not need this templating
    static_assert(std::is_same<T, uint8_t>::value || std::is_same<T, uint8_t const>::value);

    ShaderReflectionData::Structure const& structure{ pushConstant.type };

    {
        ImGui::BeginTable("Push Constant Reflection Data", 2
            , ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg
        );

        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        int32_t const columnIndexProperty{ 0 };
        int32_t const columnIndexValue{ 1 };

        ImGui::TableHeadersRow();
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(columnIndexProperty);
            ImGui::Text("Layout Byte Offset");
            ImGui::TableSetColumnIndex(columnIndexValue);
            ImGui::Text(fmt::format("{}", pushConstant.layoutOffsetBytes).c_str());
        }
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(columnIndexProperty);
            ImGui::Text("Byte Size");
            ImGui::TableSetColumnIndex(columnIndexValue);
            ImGui::Text(fmt::format("{}", structure.sizeBytes).c_str());
        }

        ImGui::EndTable();
    }

    {
        ImGui::BeginTable("Push Constant Table", 6
            , ImGuiTableFlags_Resizable 
            | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH 
            | ImGuiTableFlags_RowBg
        );

        ImGui::TableSetupColumn("Member Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Padded", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        int32_t const memberNameIndex{ 0 };
        int32_t const columnIndexValue{ 1 };
        int32_t const typeIndex{ 2 };
        int32_t const offsetIndex{ 3 };
        int32_t const columnIndexSize{ 4 };
        int32_t const columnIndexPaddedSize{ 5 };

        // Members can be sparse, with implied padding between them.
        // We track those bytes so we can print to the UI that there is padding.
        uint32_t lastByte{ 0 };
        for (ShaderReflectionData::StructureMember const& member : structure.members)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(offsetIndex);
            ImGui::Text(fmt::format("{}", member.offsetBytes).c_str());

            ImGui::TableSetColumnIndex(memberNameIndex);
            ImGui::Text(member.name.c_str());

            ImGui::TableSetColumnIndex(columnIndexSize);
            ImGui::Text(fmt::format("{}", member.type.sizeBytes).c_str());

            ImGui::TableSetColumnIndex(columnIndexPaddedSize);
            ImGui::Text(fmt::format("{}", member.type.paddedSizeBytes).c_str());

            std::visit(overloaded{
                [&](ShaderReflectionData::UnsupportedType const& unsupportedType) {
                    ImGui::Text(fmt::format("Unsupported member \"{}\"", member.name).c_str());
                },
                [&](ShaderReflectionData::Pointer const& pointerType) {
                    // Pointers should be 8 bytes, I am not sure when this would fail
                    assert(member.type.sizeBytes == 8);

                    std::string const memberLabel{ fmt::format("##{}", member.name) };

                    T* const pDataPointer{ &backingData[member.offsetBytes] };

                    ImGui::TableSetColumnIndex(columnIndexValue);

                    ImGui::BeginDisabled(readOnly);
                    {
                        ImGui::PushItemWidth(-FLT_MIN); // This hides the label
                        ImGui::InputScalar(
                            memberLabel.c_str(),
                            ImGuiDataType_U64,
                            reinterpret_cast<void*>(const_cast<uint8_t*>(pDataPointer))
                        );
                        ImGui::PopItemWidth();
                    }
                    ImGui::EndDisabled();

                    ImGui::TableSetColumnIndex(typeIndex);
                    ImGui::Text("Pointer");
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
                                bSupportedType = false;
                                break;
                            }
                        },
                    }, numericType.componentType);

                    ImGui::TableSetColumnIndex(typeIndex);
                    ImGui::Text("Numeric Type");

                    if (!bSupportedType)
                    {
                        ImGui::TableSetColumnIndex(columnIndexValue);
                        ImGui::Text(
                            fmt::format("Unsupported component bit width {} for member {}", numericType.componentBitWidth, member.name).c_str()
                        );
                        return;
                    }

                    // SPIR-V aggregate types are column major. 
                    // We render each "column" of the spirv data type as a "row" of imgui inputs to avoid flipping.
                    for (uint32_t column{ 0 }; column < columns; column++)
                    {
                        size_t const byteOffset{ column * rows * numericType.componentBitWidth / 8 + member.offsetBytes };
                        T* const pDataPointer{ &backingData[byteOffset] };

                        // Check that ImGui won't modify out of bounds data
                        assert((byteOffset + rows * numericType.componentBitWidth / 8) <= backingData.size());

                        std::string const rowLabel{ fmt::format("##{}{}", member.name, column) };

                        ImGui::TableSetColumnIndex(columnIndexValue);
                        ImGui::BeginDisabled(readOnly);
                        {
                            ImGui::PushItemWidth(-FLT_MIN); // This hides the label
                            ImGui::InputScalarN(
                                rowLabel.c_str(),
                                imguiDataType,
                                reinterpret_cast<void*>(const_cast<uint8_t*>(pDataPointer)),
                                rows
                            );
                            ImGui::PopItemWidth();
                        }
                        ImGui::EndDisabled();
                    }
                },
                }, member.type.typeData);
        }

        ImGui::EndTable();
    }
}

/*
* Type-safe wrappers that call the methods that use the shader reflection data to render a push constant.
*/
template<>
void imguiPipelineControls(InstancedMeshGraphicsPipeline const& pipeline)
{
    std::span<uint8_t const> const pushConstantBytes{ 
        reinterpret_cast<uint8_t const*>(&pipeline.pushConstant())
        , sizeof(pipeline.pushConstant())
    };
}

template<>
void imguiPipelineControls(BackgroundComputePipeline const& pipeline)
{
    std::span<uint8_t const> const pushConstantBytes{
        reinterpret_cast<uint8_t const*>(&pipeline.pushConstant())
        , sizeof(pipeline.pushConstant())
    };

    ImGui::Separator();
    ImGui::Text("Sky Shader");

    imguiPushStructureControl(pipeline.pushConstantReflected(), true, pushConstantBytes);
}

template<>
void imguiPipelineControls(GenericComputePipeline& pipeline)
{
    size_t const currentShaderIndex{ pipeline.shaderIndex() };

    size_t index{ 0 };
    for (ShaderObjectReflected const& shader : pipeline.shaders())
    {
        if (ImGui::Button(fmt::format("{}##shader{}", shader.name(), index).c_str()))
        {
            pipeline.selectShader(index);
        }
        index += 1;
    }

    ImGui::Separator();

    ShaderObjectReflected const& currentShader{ pipeline.currentShader() };
    ShaderReflectionData const& reflectionData{ currentShader.reflectionData() };

    ImGui::Text(currentShader.name().c_str());

    if (reflectionData.defaultEntryPointHasPushConstant())
    {
        imguiPushStructureControl(reflectionData.defaultPushConstant(), false, pipeline.mapPushConstantBytes());
    }
    else
    {
        ImGui::Text("No push constants.");
    }
}