#pragma once

#include <imgui.h>
#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <span>
#include <optional>

struct FloatBounds
{
    float min{ -FLT_MAX };
    float max{ FLT_MAX };

};

struct PropertySliderBehavior
{
    float const speed{ 0.0f };
    ImGuiSliderFlags const flags{ ImGuiSliderFlags_None };
    FloatBounds bounds{ .min{ -FLT_MAX }, .max{ FLT_MAX } };
};

struct PropertyTable
{
private:
    typedef PropertyTable Self;

    // Use 16 bit uints, since DearImGui uses 32 bit ints but we only use positive values.
    static uint16_t constexpr PROPERTY_INDEX{ 0 };
    static uint16_t constexpr VALUE_INDEX{ 1 };
    static uint16_t constexpr RESET_INDEX{ 2 };

    uint16_t const m_styleVariablesCount{ 0 };

    // Used to avoid name collision, by incrementing and salting names passed to ImGui.
    size_t m_propertyCount{ 0 };

    bool m_open{ false };
    bool m_rowOpen{ false };

    size_t m_childPropertyDepth{ 0 };

    // The depth at which we first collapsed. If no value is set, we are not collapsed.
    // We track this so when nesting child properties within a collapsed child, we can see when to stop being
    // collapsed.
    std::optional<size_t> m_childPropertyFirstCollapse{ std::nullopt };

    PropertyTable() = delete;
    explicit PropertyTable(uint16_t styleVariables)
        : m_styleVariablesCount(styleVariables)
        , m_open(true)
    {}

    void nameColumn(
        std::string name
    );

    bool resetColumn(
        std::string name
        , bool visible
    );

    static float collapseButtonWidth()
    {
        return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
    }

    void checkInvariant() const
    {
        // If we are collapsed, it must have occured at the current or an earlier depth.
        // Violation of this invariant likely means m_childPropertyDepth was decremented without updating collapse
        // status.
        assert(
            !m_childPropertyFirstCollapse.has_value()
            || m_childPropertyFirstCollapse.value() <= m_childPropertyDepth
            && "PropertyTable collapse depth invariant violated."
        );
    }

    bool hideNextRow() const 
    { 
        return m_childPropertyFirstCollapse.has_value() 
            && m_childPropertyDepth > m_childPropertyFirstCollapse.value(); 
    }

    // If this returns false, the row should not be modified further. Do NOT call rowEnd if this returns false.
    bool rowBegin(std::string const& name);
    void rowEnd();

public:
    // Creates a separate window, that demonstrates PropertyTable usage.
    static void demoWindow();

    // Using the default name synchronizes the table's column widths (and possibly other properties) across the window.
    static PropertyTable begin(std::string name = "PropertyTable");

    void end();

    // Adds an arrow button to the previous row, and enters a collapsible section.
    // Further calls to row drawing methods will be skipped until childPropertyEnd is called,
    // depending on if this rows button is collapsed or not. This is tracked internally.
    // This also adds proper styling and indenting.
    PropertyTable& childPropertyBegin();
    // This adds a new row for the collapsing button. See PropertyTable::childPropertyBegin.
    PropertyTable& rowChildPropertyBegin(std::string const& name);

    // A corresponding PropertyTable::rowChildPropertyBegin or PropertyTable::childPropertyBegin must have been called.
    PropertyTable& childPropertyEnd();

    // Adds a row that contains a dropdown, containing a list of values, with a reset button.
    PropertyTable& rowDropdown(
        std::string const& name
        , size_t& selectedIndex
        , size_t const& defaultIndex
        , std::span<std::string const> displayValues
    );

    // Adds a row that contains an interactable text entry, alongside a reset button.
    PropertyTable& rowText(
        std::string const& name
        , std::string& value
        , std::string const& resetValue
    );

    // Adds a row that contains some read only text.
    PropertyTable& rowReadOnlyText(
        std::string const& name
        , std::string const& value
    );

    // Adds a row that contains an interactable 32-bit signed integer entry, alongside a reset button.
    PropertyTable& rowInteger(
        std::string const& name
        , int32_t& value
        , int32_t const& resetValue
        , PropertySliderBehavior const behavior
    );

    // Adds a row that contains a read only integer.
    PropertyTable& rowReadOnlyInteger(
        std::string const& name
        , int32_t const& value
    );

    // Adds a row that contains an interactable three-float vector entry, alongside a reset button.
    PropertyTable& rowVec3(
        std::string const& name
        , glm::vec3& value
        , glm::vec3 const& resetValue
        , PropertySliderBehavior const behavior
    );

    // Adds a row that contains a non-interactable three-float vector entry.
    PropertyTable& rowReadOnlyVec3(
        std::string const& name
        , glm::vec3 const& value
    );

    // Adds a row that contains an interactable float entry, alongside a reset button.
    PropertyTable& rowFloat(
        std::string const& name
        , float& value
        , float const& resetValue
        , PropertySliderBehavior const behavior
    );

    // Adds a row that contains a non-interactable float entry.
    PropertyTable& rowReadOnlyFloat(
        std::string const& name
        , float const& value
    );

    // Adds a row that contains an interactable boolean entry, alongside a reset button.
    PropertyTable& rowBoolean(
        std::string const& name
        , bool& value
        , bool const& resetValue
    );

    // Adds a row that contains a non-interactable float entry.
    PropertyTable& rowReadOnlyBoolean(
        std::string const& name
        , bool const& value
    );
};

