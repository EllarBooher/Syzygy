#include "dockinglayout.hpp"

#include <imgui_internal.h>

ui::DockingLayout
ui::buildDefaultMultiWindowLayout(ui::UIRectangle workArea, ImGuiID parentNode)
{
    ImGui::DockBuilderAddNode(parentNode);

    // Set the size and position:
    ImGui::DockBuilderSetNodeSize(parentNode, workArea.size());
    ImGui::DockBuilderSetNodePos(parentNode, workArea.pos());

    ImGuiID parentID{parentNode};

    ImGuiID const leftID{ImGui::DockBuilderSplitNode(
        parentID, ImGuiDir_Left, 0.3, nullptr, &parentID
    )};

    ImGuiID const rightID{ImGui::DockBuilderSplitNode(
        parentID, ImGuiDir_Right, 0.2 / (1.0 - 0.3), nullptr, &parentID
    )};

    ImGuiID const centerBottomID{ImGui::DockBuilderSplitNode(
        parentID, ImGuiDir_Down, 0.2, nullptr, &parentID
    )};

    ImGuiID const centerTopID{parentID};

    ImGui::DockBuilderFinish(parentNode);

    return DockingLayout{
        .left = leftID,
        .right = rightID,
        .centerBottom = centerBottomID,
        .centerTop = centerTopID,
    };
}