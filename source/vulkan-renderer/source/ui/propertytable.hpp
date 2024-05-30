#pragma once

#include <imgui.h>
#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <span>

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

    static uint16_t constexpr PROPERTY_INDEX{ 0 };
    static uint16_t constexpr VALUE_INDEX{ 1 };
    static uint16_t constexpr RESET_INDEX{ 2 };

    uint16_t const m_styleVariablesCount{ 0 };

    PropertyTable() = delete;
    explicit PropertyTable(uint16_t styleVariables)
        : m_styleVariablesCount(styleVariables)
    {}

    void nameColumn(
        std::string name
    );

    bool resetColumn(
        std::string name
        , bool visible
    );

public:
    static PropertyTable begin(std::string name = "PropertyTable");

    void end();

    PropertyTable& rowChildProperty(
        std::string const& name
        , bool& collapsed
    );

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


