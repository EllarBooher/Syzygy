#include "uiwidgets.hpp"

#include "syzygy/editor/uilayer.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/ui/propertytable.hpp"
#include <imgui.h>
#include <spdlog/fmt/bundled/core.h>
#include <utility>

namespace syzygy
{
void UIWidget::draw()
{
    ImVec2 const contentRegion{ImGui::GetIO().DisplaySize};

    ImGui::SetNextWindowSize(
        ImVec2{contentRegion.x / 2, contentRegion.y / 2}, ImGuiCond_Appearing
    );
    ImGui::SetNextWindowBgAlpha(1.0F);

    if (m_open)
    {
        ImGui::OpenPopup(
            fmt::format(
                "{}##{}", m_specification.title, static_cast<uint64_t>(m_id)
            )
                .c_str(),
            ImGuiPopupFlags_None
        );
    }

    if (ImGui::BeginPopupModal(
            fmt::format(
                "{}##{}", m_specification.title, static_cast<uint64_t>(m_id)
            )
                .c_str(),
            &m_open
        ))
    {
        renderContents();
        ImGui::EndPopup();
    }
}
auto UIWidget::shouldClose() const -> bool { return !m_open; }
void UIWidget::close() { cleanup(); }
void UIWidget::moveNonVirtualMembers(UIWidget&& other)
{
    m_open = std::exchange(other.m_open, false);
    m_specification = std::exchange(other.m_specification, {});
}
} // namespace syzygy

namespace syzygy
{
ImageLoaderWidget::ImageLoaderWidget(ImageLoaderWidget&& other) noexcept
{
    *this = std::move(other);
}
auto ImageLoaderWidget::operator=(ImageLoaderWidget&& other) noexcept
    -> ImageLoaderWidget&
{
    moveNonVirtualMembers(std::move(static_cast<UIWidget&>(other)));

    m_task = std::move(other.m_task);

    return *this;
}
auto ImageLoaderWidget::create(
    UILayer& windowDestination,
    std::span<std::filesystem::path const> const paths
) -> std::shared_ptr<ImageLoadingTask>
{
    ImageLoaderWidget widget{};

    widget.m_open = true;
    widget.m_specification = UIWindowSpecification{
        .title = "Texture Import Settings",
    };

    widget.m_task = std::make_shared<ImageLoadingTask>(ImageLoadingTask{});
    ImageLoadingTask& task = *widget.m_task;

    std::shared_ptr<ImageLoadingTask> resultHandle{widget.m_task};

    task.status = TaskStatus::Waiting;
    task.loadees.reserve(paths.size());

    for (auto const& path : paths)
    {
        task.loadees.push_back(ImageDiskSource{
            .path = path,
            .nonlinearEncoding = false,
        });
    }

    windowDestination.addWidget(
        std::make_unique<ImageLoaderWidget>(std::move(widget))
    );

    return resultHandle;
}
void ImageLoaderWidget::renderContents()
{
    if (m_task == nullptr)
    {
        ImGui::Text("Error: No active task.");
        return;
    }

    ImageLoadingTask& task = *m_task;

    if (ImGui::Button("Submit"))
    {
        m_open = false;
        task.status = TaskStatus::Success;
    }
    ImGui::SameLine();
    ImGui::Text("Application will likely hang when processing many images.");
    if (ImGui::Button("Cancel"))
    {
        m_open = false;
        task.status = TaskStatus::Cancelled;
    }

    if (PropertyTable table{PropertyTable::begin()}; table.open())
    {
        for (ImageDiskSource& loadee : task.loadees)
        {
            table.rowChildPropertyBegin(loadee.path.stem().string(), false);
            table.rowBoolean(
                "Nonlinear Encoding", loadee.nonlinearEncoding, false
            );
            table.rowReadOnlyTextInput(
                "Path on Disk", loadee.path.string(), false
            );
            table.childPropertyEnd();
        }
        table.end();
    }
}
void ImageLoaderWidget::cleanup()
{
    if (m_task != nullptr)
    {
        m_task->status = TaskStatus::Cancelled;
    }
}
} // namespace syzygy
