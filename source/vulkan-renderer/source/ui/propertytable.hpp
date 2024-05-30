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

    bool m_open{ false };

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

public:
    // Using a default name synchronizes the tables across the window.
    static PropertyTable begin(std::string name = "PropertyTable");

    void end();

    // Adds an arrow button. 
    // Further calls to row drawing methods will be skipped until rowChildPropertyEnd is called,
    // depending on if the resulting button is collapsed or not.
    PropertyTable& rowChildPropertyBegin(std::string const& name);

    // There must have been a corresponding call to rowChildPropertyBegin before calling this method.
    PropertyTable& rowChildPropertyEnd();

    PropertyTable& rowDropdown(
        std::string const& name
        , size_t& selectedIndex
        , size_t const& defaultIndex
        , std::span<std::string const> displayValues
    );

    PropertyTable& rowReadOnlyText(
        std::string const& name
        , std::string const& value
    );

    PropertyTable& rowReadOnlyInteger(
        std::string const& name
        , int32_t const& value
    );

    PropertyTable& rowVec3(
        std::string const& name
        , glm::vec3& value
        , glm::vec3 const& resetValue
        , PropertySliderBehavior const behavior
    );

    PropertyTable& rowReadOnlyVec3(
        std::string const& name
        , glm::vec3 const& value
    );

    PropertyTable& rowFloat(
        std::string const& name
        , float& value
        , float const& resetValue
        , PropertySliderBehavior const behavior
    );

    PropertyTable& rowReadOnlyFloat(
        std::string const& name
        , float const& value
    );

    PropertyTable& rowBoolean(
        std::string const& name
        , bool& value
        , bool const& resetValue
    );

    PropertyTable& rowReadOnlyBoolean(
        std::string const& name
        , bool const& value
    );

    static void demoWindow();
};


