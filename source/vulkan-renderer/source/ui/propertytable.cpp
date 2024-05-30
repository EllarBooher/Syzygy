#include "propertytable.hpp"

#include "imgui_internal.h"

#include <unordered_map>

void PropertyTable::nameColumn(std::string const name)
{
    ImGui::TableSetColumnIndex(PROPERTY_INDEX);
    ImGui::Text(name.c_str());
}

bool PropertyTable::resetColumn(std::string const name, bool visible)
{
    ImGui::TableSetColumnIndex(RESET_INDEX);

    if (!visible) return false;

    float const width{
        ImGui::GetColumnWidth(RESET_INDEX)
    };
    ImGui::SetNextItemWidth(width);

    bool const clicked{
        ImGui::Button(fmt::format("<-##{}reset", name).c_str(), ImVec2{ -1.0f,0.0f })
    };

    return clicked;
}

PropertyTable PropertyTable::begin(std::string const name)
{
    ImGui::BeginTable(
        name.c_str()
        , 3
        , ImGuiTableFlags_None
        | ImGuiTableFlags_BordersInner
        | ImGuiTableFlags_Resizable
    );

    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize);
    ImGui::TableSetupColumn(
        "Reset"
        , ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize
        , ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::CalcTextSize("<-").x
    );

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{ 0.0f, 6.0f });

    uint16_t const styleVariableCount{ 1 };

    return PropertyTable(styleVariableCount);
}

void PropertyTable::end()
{
    assert(m_open && "end() called on PropertyTable that was not open.");

    m_open = false;

    ImGui::PopStyleVar(m_styleVariablesCount);
    ImGui::EndTable();
}

PropertyTable& PropertyTable::rowChildPropertyBegin(std::string const& name)
{
    static std::unordered_map<ImGuiID, bool> collapseStatus{};

    checkInvariant();

    ImGui::TableNextRow();

    bool const hideRow{ hideNextRow() };
    if (!hideRow)
    {
        Self::nameColumn(name);

        ImGui::TableSetColumnIndex(VALUE_INDEX);

        std::string const arrowButtonName{
            fmt::format("{}##arrowButton", name)
        };
        ImGuiID const arrowButtonID{
            ImGui::GetID(arrowButtonName.c_str())
        };

        bool& collapsed{ collapseStatus[arrowButtonID] };
        ImGuiDir const direction{
            collapsed
            ? ImGuiDir_Right
            : ImGuiDir_Down
        };

        if (ImGui::ArrowButton(arrowButtonName.c_str(), direction))
        {
            collapsed = !collapsed;
        }

        if (!m_childPropertyFirstCollapse.has_value() && collapsed)
        {
            m_childPropertyFirstCollapse = m_childPropertyDepth;
        }
    }
    
    m_childPropertyDepth += 1;
    ImGui::Indent(ImGui::GetStyle().IndentSpacing);

    return *this;
}

PropertyTable& PropertyTable::rowChildPropertyEnd()
{
    checkInvariant();
    assert(
        m_childPropertyDepth > 0
        && "rowChildPropertyEnd() called on PropertyTable with no matching rowChildPropertyBegin()"
    );

    m_childPropertyDepth -= 1;
    ImGui::Unindent(ImGui::GetStyle().IndentSpacing);

    if (m_childPropertyFirstCollapse.has_value()
        && m_childPropertyFirstCollapse.value() >= m_childPropertyDepth)
    {
        m_childPropertyFirstCollapse = std::nullopt;
    }

    return *this;
}

PropertyTable& PropertyTable::rowDropdown(
    std::string const& name
    , size_t& selectedIndex
    , size_t const& defaultIndex
    , std::span<std::string const> displayValues
)
{
    checkInvariant();

    if (hideNextRow()) return *this;

    ImGui::TableNextRow();

    Self::nameColumn(name);

    if (selectedIndex >= displayValues.size())
    {
        selectedIndex = 0;
    }

    std::string const& previewValue{ 
        displayValues.empty() 
        ? "No Possible Values." 
        : displayValues[selectedIndex] 
    };

    ImGui::TableSetColumnIndex(VALUE_INDEX);

    ImGui::BeginDisabled(displayValues.empty());
    if (ImGui::BeginCombo(fmt::format("##{}combo", name).c_str(), previewValue.c_str()))
    {
        size_t index{ 0 };
        for (std::string const& displayValue : displayValues)
        {
            bool isSelected{ index == selectedIndex };
            if (ImGui::Selectable(displayValue.c_str(), isSelected))
            {
                selectedIndex = index;
            }

            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }

            index += 1;
        }
        ImGui::EndCombo();
    }

    ImGui::EndDisabled();

    if (Self::resetColumn(name, selectedIndex != defaultIndex))
    {
        selectedIndex = defaultIndex;
    }

    return *this;
}

PropertyTable& PropertyTable::rowReadOnlyText(std::string const& name, std::string const& value)
{
    checkInvariant();

    if (hideNextRow()) return *this;

    ImGui::TableNextRow();

    Self::nameColumn(name);

    ImGui::TableSetColumnIndex(VALUE_INDEX);
    ImGui::SetNextItemWidth(ImGui::GetColumnWidth(VALUE_INDEX));
    ImGui::TextWrapped(value.c_str());

    return *this;
}

PropertyTable& PropertyTable::rowReadOnlyInteger(
    std::string const& name
    , int32_t const& value
)
{
    checkInvariant();

    if (hideNextRow()) return *this;

    ImGui::TableNextRow();

    Self::nameColumn(name);

    ImGui::TableSetColumnIndex(VALUE_INDEX);
    ImGui::BeginDisabled();

    int32_t valueCopy{ value };
    ImGui::DragInt(
        fmt::format("##{}", name).c_str()
        , &valueCopy
        , 0
        , 0
        , 0
        , "%u"
        , ImGuiSliderFlags_None
    );

    ImGui::EndDisabled();

    return *this;
}

PropertyTable& PropertyTable::rowVec3(std::string const& name, glm::vec3& value, glm::vec3 const& resetValue, PropertySliderBehavior const behavior)
{
    checkInvariant();

    if (hideNextRow()) return *this;

    ImGui::TableNextRow();

    Self::nameColumn(name);

    ImGui::TableSetColumnIndex(VALUE_INDEX);
    ImGui::PushMultiItemsWidths(3, ImGui::GetColumnWidth(VALUE_INDEX));
    for (uint32_t component{ 0 }; component < 3; component++)
    {
        float const spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
        if (component > 0) ImGui::SameLine(0.0f, spacing);

        ImGui::DragFloat(
            fmt::format("##{}{}", name, component).c_str()
            , &value[component]
            , behavior.speed
            , behavior.bounds.min
            , behavior.bounds.max
            , "%.6f"
            , behavior.flags
        );
        ImGui::PopItemWidth();
    }

    if (Self::resetColumn(name, value != resetValue))
    {
        value = resetValue;
    }

    return *this;
}

PropertyTable& PropertyTable::rowReadOnlyVec3(std::string const& name, glm::vec3 const& value)
{
    checkInvariant();

    if (hideNextRow()) return *this;

    ImGui::TableNextRow();

    Self::nameColumn(name);

    ImGui::TableSetColumnIndex(VALUE_INDEX);

    ImGui::BeginDisabled();

    ImGui::PushMultiItemsWidths(3, ImGui::GetColumnWidth(VALUE_INDEX));
    for (uint32_t component{ 0 }; component < 3; component++)
    {
        float const interComponentSpacing{ ImGui::GetStyle().ItemInnerSpacing.x };
        if (component > 0) ImGui::SameLine(0.0f, interComponentSpacing);

        float componentValue{ value[component] };
        ImGui::DragFloat(
            fmt::format("##{}{}", name, component).c_str()
            , &componentValue
            , 0.0f
            , 0.0f
            , 0.0f
            , "%.6f"
            , ImGuiSliderFlags_None
        );
        ImGui::PopItemWidth();
    }
    ImGui::EndDisabled();

    return *this;
}

PropertyTable& PropertyTable::rowFloat(std::string const& name, float& value, float const& resetValue, PropertySliderBehavior const behavior)
{
    checkInvariant();

    if (hideNextRow()) return *this;

    ImGui::TableNextRow();

    Self::nameColumn(name);

    ImGui::TableSetColumnIndex(VALUE_INDEX);
    ImGui::DragFloat(
        fmt::format("##{}", name).c_str()
        , &value
        , behavior.speed
        , behavior.bounds.min
        , behavior.bounds.max
        , "%.6f"
        , behavior.flags
    );

    if (Self::resetColumn(name, value != resetValue))
    {
        value = resetValue;
    }

    return *this;
}

PropertyTable& PropertyTable::rowReadOnlyFloat(std::string const& name, float const& value)
{
    checkInvariant();

    if (hideNextRow()) return *this;

    ImGui::TableNextRow();

    Self::nameColumn(name);

    ImGui::TableSetColumnIndex(VALUE_INDEX);

    ImGui::BeginDisabled();

    float valueCopy{ value };
    ImGui::DragFloat(
        fmt::format("##{}", name).c_str()
        , &valueCopy
        , 0.0
        , 0.0
        , 0.0
        , "%.6f"
        , ImGuiSliderFlags_None
    );

    ImGui::EndDisabled();

    return *this;
}

PropertyTable& PropertyTable::rowBoolean(std::string const& name, bool& value, bool const& resetValue)
{
    checkInvariant();

    if (hideNextRow()) return *this;

    ImGui::TableNextRow();

    Self::nameColumn(name);

    ImGui::TableSetColumnIndex(VALUE_INDEX);
    ImGui::Checkbox(fmt::format("##{}", name).c_str(), &value);

    if (Self::resetColumn(name, value != resetValue))
    {
        value = resetValue;
    }

    return *this;
}

PropertyTable& PropertyTable::rowReadOnlyBoolean(std::string const& name, bool const& value)
{
    checkInvariant();

    if (hideNextRow()) return *this;

    ImGui::TableNextRow();

    Self::nameColumn(name);

    ImGui::TableSetColumnIndex(VALUE_INDEX);

    ImGui::BeginDisabled();

    bool valueCopy{ value };
    ImGui::Checkbox(fmt::format("##{}", name).c_str(), &valueCopy);

    ImGui::EndDisabled();

    return *this;
}

void PropertyTable::demoWindow()
{
    if (!ImGui::Begin("Property Table Demo Window"))
    {
        ImGui::End();
        return;
    }

    static bool valueBoolean{ false };
    static float valueBoundedFloat{ 0.0f };
    static float valueUnboundedFloat{ 0.0f };
    static glm::vec3 valueBoundedVec3{ 0.0f };
    static glm::vec3 valueUnboundedVec3{ 0.0f };

    static float minimumBound{ -1.0f };
    static float maximumBound{ 1.0f };

    PropertyTable::begin("Demo Table")
        .rowBoolean(
            "Boolean"
            , valueBoolean, false)
        .rowReadOnlyBoolean(
            "Read-Only Boolean"
            , true)
        .rowFloat(
            "Bounds Minimum"
            , minimumBound, -1.0f
            , PropertySliderBehavior{ 
                .speed{1.0f} 
            })
        .rowFloat(
            "Bounds Maximum"
            , maximumBound, 1.0f
            , PropertySliderBehavior{
                .speed{1.0f}
            })
        .rowFloat(
            "Bounded Float"
            , valueBoundedFloat, 0.0f
            , PropertySliderBehavior{
                .bounds{ minimumBound, maximumBound },
            })
        .rowVec3(
            "Bounded Vec3"
            , valueBoundedVec3, glm::vec3{ 0.0f }
            , PropertySliderBehavior{
                .bounds{ minimumBound, maximumBound },
            })
        .rowFloat(
            "Unbounded Float"
            , valueUnboundedFloat, 0.0f
            , PropertySliderBehavior{
                .speed{ 1.0f },
            })
        .rowVec3(
            "Unbounded Vec3"
            , valueUnboundedVec3, glm::vec3{ 0.0f }
            , PropertySliderBehavior{
                .speed{ 0.1f },
            })
        .rowReadOnlyFloat(
            "Read Only Float"
            , 1.0f)
        .rowReadOnlyVec3(
            "Read-Only Vec3"
            , glm::vec3{ 1.0f })
        .end();

    ImGui::End(); // End window
}
