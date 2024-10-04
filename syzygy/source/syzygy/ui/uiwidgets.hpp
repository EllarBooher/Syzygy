#pragma once

#include "imgui.h"
#include "syzygy/core/uuid.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace syzygy
{
struct UILayer;
} // namespace syzygy

namespace syzygy
{
struct UIWindowSpecification
{
    std::string title{};
    std::optional<ImGuiID> dockNode{};
};

// Stateful widgets that require persistance
struct UIWidget
{
public:
    UIWidget(UIWidget&&) = delete;
    auto operator=(UIWidget&&) -> UIWidget& = delete;

    UIWidget(UIWidget const&) = delete;
    auto operator=(UIWidget const&) -> UIWidget& = delete;

    void draw();
    [[nodiscard]] auto shouldClose() const -> bool;
    void close();

    virtual ~UIWidget() = default;

protected:
    UIWidget() = default;
    void moveNonVirtualMembers(UIWidget&& other);

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    bool m_open{};

    // TODO: Determine what should be stored in UIWindowScope instead
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    UIWindowSpecification m_specification;

private:
    // A unique ID used to avoid name collisions without child classes
    // needing to know how to do that.
    UUID m_id{UUID::createNew()};

    virtual void renderContents() = 0;
    virtual void cleanup() = 0;
};

enum class TaskStatus
{
    Waiting,
    Success,
    Cancelled
};

struct ImageDiskSource
{
    std::filesystem::path path{};
    bool nonlinearEncoding{};
};

struct ImageLoadingTask
{
    TaskStatus status{};
    std::vector<ImageDiskSource> loadees{};
};

struct ImageLoaderWidget : UIWidget
{
    ImageLoaderWidget(ImageLoaderWidget&&) noexcept;
    auto operator=(ImageLoaderWidget&&) noexcept -> ImageLoaderWidget&;

    static auto create(UILayer&, std::span<std::filesystem::path const> paths)
        -> std::shared_ptr<ImageLoadingTask>;

    ~ImageLoaderWidget() override = default;

private:
    ImageLoaderWidget() = default;

    // Begin UIWidget interface
    void renderContents() override;
    void cleanup() override;
    // End UIWidget interface

    std::shared_ptr<ImageLoadingTask> m_task{};
};
} // namespace syzygy