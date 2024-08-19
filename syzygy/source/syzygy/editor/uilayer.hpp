#pragma once

namespace syzygy
{
struct UILayer
{
public:
    UILayer(UILayer const&) = delete;
    UILayer& operator=(UILayer const&) = delete;

    auto operator=(UILayer&&) noexcept -> UILayer&;
    UILayer(UILayer&&) noexcept;
    ~UILayer();

private:
    UILayer() = default;
    void destroy();

public:
    static auto create(VkInstance const)
};
} // namespace syzygy