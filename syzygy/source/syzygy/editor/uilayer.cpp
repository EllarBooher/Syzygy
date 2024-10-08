#include "uilayer.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/editor/window.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/scenetexture.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include "syzygy/ui/statelesswidgets.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include "syzygy/ui/uiwidgets.hpp"
#include <algorithm>
#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/ext/vector_relational.hpp>
#include <glm/vec3.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <implot.h>
#include <span>
#include <utility>
#include <vector>

namespace syzygy
{
void uiReload(VkDevice const device, UIPreferences const preferences)
{
    float constexpr FONT_BASE_SIZE{13.0F};

    ImFontConfig fontConfig{};
    fontConfig.SizePixels = FONT_BASE_SIZE * preferences.dpiScale;
    fontConfig.OversampleH = 1;
    fontConfig.OversampleV = 1;
    fontConfig.PixelSnapH = true;

    ImGui::GetIO().Fonts->Clear();
    ImGui::GetIO().Fonts->AddFontDefault(&fontConfig);

    // Wait for idle since we are modifying backend resources
    vkDeviceWaitIdle(device);
    // We destroy this to later force a rebuild when the fonts are needed.
    ImGui_ImplVulkan_DestroyFontsTexture();

    // TODO: is rebuilding the font with a specific scale good?
    // ImGui recommends building fonts at various sizes then just
    // selecting them

    // Reset style so further scaling works off the base "1.0x" scaling
    // TODO: Resetting is problematic since not all fields are sizes, changes we
    // have made might be overwritten. A more comprehensive fix is needed.
    ImGuiStyle newStyle{};
    std::copy(
        std::begin(ImGui::GetStyle().Colors),
        std::end(ImGui::GetStyle().Colors),
        std::begin(newStyle.Colors)
    );
    newStyle.ScaleAllSizes(preferences.dpiScale);
    ImGui::GetStyle() = newStyle;
}

UILayer::UILayer(UILayer&& other) noexcept { *this = std::move(other); }

auto UILayer::operator=(UILayer&& other) noexcept -> UILayer&
{
    m_backendInitialized = std::exchange(other.m_backendInitialized, false);

    m_reloadNecessary = std::exchange(other.m_reloadNecessary, false);
    m_currentPreferences =
        std::exchange(other.m_currentPreferences, UIPreferences{});
    m_defaultPreferences =
        std::exchange(other.m_defaultPreferences, UIPreferences{});

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_imguiPool = std::exchange(other.m_imguiPool, VK_NULL_HANDLE);

    m_currentHUD = std::exchange(other.m_currentHUD, HUDState{});
    m_currentDockingLayout =
        std::exchange(other.m_currentDockingLayout, DockingLayout{});

    m_sceneTexture = std::move(other.m_sceneTexture);
    m_imguiSceneTextureHandle =
        std::exchange(other.m_imguiSceneTextureHandle, nullptr);
    m_outputTexture = std::move(other.m_outputTexture);

    return *this;
}

UILayer::~UILayer() { destroy(); }
UILayer::UILayer() = default;

void UILayer::destroy()
{
    if (m_backendInitialized)
    {
        ImPlot::DestroyContext();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        m_backendInitialized = false;
    }

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_imguiPool, nullptr);
        // m_imguiSceneTextureHandle is freed here
    }
    else if (m_imguiPool != VK_NULL_HANDLE)
    {
        SZG_WARNING("UILayer: Device was NULL when pool was not. The pool was "
                    "likely leaked.");
    }

    m_sceneTexture.reset();
    m_outputTexture.reset();

    m_device = VK_NULL_HANDLE;

    m_reloadNecessary = false;
    m_currentPreferences = {};
    m_defaultPreferences = {};

    m_currentHUD = {};
    m_currentDockingLayout = {};
}

auto syzygy::UILayer::create(
    VkInstance const instance,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VmaAllocator const allocator,
    VkExtent2D textureCapacity,
    uint32_t const graphicsQueueFamily,
    VkQueue const graphicsQueue,
    PlatformWindow& mainWindow,
    UIPreferences const defaultPreferences
) -> std::optional<UILayer>
{
    std::optional<UILayer> layerResult{UILayer{}};
    UILayer& layer{layerResult.value()};

    std::vector<VkDescriptorPoolSize> const poolSizes{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo const poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,

        .maxSets = 1000,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    SZG_TRY_VK(
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &layer.m_imguiPool),
        "Failed to create descriptor pool for Dear ImGui",
        std::nullopt
    );

    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGui::StyleColorsDark();
    for (ImVec4& styleColor : ImGui::GetStyle().Colors)
    {
        // We linearize the colors, since ImGui seems to have picked its colors
        // such that they look best when interpreted as non-linear

        // Transfer implementation as defined in
        // https://www.color.org/chardata/rgb/srgb.xalter

        glm::vec3 const rgb{styleColor.x, styleColor.y, styleColor.z};
        glm::bvec3 const linearCutoff{
            glm::lessThan(rgb, glm::vec3(0.0031308F * 12.92F))
        };
        glm::vec3 const linear = rgb / glm::vec3(12.92F);
        glm::vec3 const nonlinear = glm::pow(
            (rgb + glm::vec3(0.055F)) / glm::vec3(1.055F), glm::vec3(2.4F)
        );

        glm::vec3 const converted{
            0.95F * glm::mix(nonlinear, linear, linearCutoff)
        };

        styleColor.x = converted.x;
        styleColor.y = converted.y;
        styleColor.z = converted.z;
    }

    ImVec4 constexpr MODAL_BACKGROUND_DIM{0.0F, 0.0F, 0.0F, 0.8F};
    ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = MODAL_BACKGROUND_DIM;

    ImGui_ImplGlfw_InitForVulkan(mainWindow.handle(), true);

    // Load functions since we are using volk,
    // and not the built-in vulkan loader
    ImGui_ImplVulkan_LoadFunctions(
        [](char const* functionName, void* vkInstance)
    {
        return vkGetInstanceProcAddr(
            *(reinterpret_cast<VkInstance*>(vkInstance)), functionName
        );
    },
        const_cast<VkInstance*>(&instance)
    );

    // This amount is recommended by ImGui to satisfy validation layers, even if
    // a little wasteful
    VkDeviceSize constexpr IMGUI_MIN_ALLOCATION_SIZE{1024ULL * 1024ULL};

    auto const checkVkResult_imgui{
        [](VkResult const result)
    {
        if (result == VK_SUCCESS)
        {
            return;
        }

        SZG_ERROR(
            "Dear ImGui Detected Vulkan Error : {}", string_VkResult(result)
        );
    },
    };

    std::vector<VkFormat> const colorAttachmentFormats{
        VK_FORMAT_R16G16B16A16_UNORM
    };
    VkPipelineRenderingCreateInfo const dynamicRenderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,

        .viewMask = 0, // Not sure on this value
        .colorAttachmentCount =
            static_cast<uint32_t>(colorAttachmentFormats.size()),
        .pColorAttachmentFormats = colorAttachmentFormats.data(),

        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = instance,
        .PhysicalDevice = physicalDevice,
        .Device = device,

        .QueueFamily = graphicsQueueFamily,
        .Queue = graphicsQueue,

        .DescriptorPool = layer.m_imguiPool,

        .MinImageCount = 3,
        .ImageCount = 3,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT, // No MSAA

        // Dynamic rendering
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = dynamicRenderingInfo,

        // Allocation/Debug
        .Allocator = nullptr,
        .CheckVkResultFn = checkVkResult_imgui,
        .MinAllocationSize = IMGUI_MIN_ALLOCATION_SIZE,
    };

    ImGui_ImplVulkan_Init(&initInfo);

    layer.m_backendInitialized = true;

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (std::optional<SceneTexture> outputTextureResult{SceneTexture::create(
            device,
            allocator,
            textureCapacity,
            VK_FORMAT_R16G16B16A16_UNORM,
            VK_FORMAT_D32_SFLOAT
        )};
        outputTextureResult.has_value())
    {
        layer.m_outputTexture = std::make_unique<SceneTexture>(
            std::move(outputTextureResult).value()
        );
    }
    else
    {
        SZG_ERROR("Failed to allocate UI Layer output texture.");
        return std::nullopt;
    }
    if (std::optional<SceneTexture> sceneTextureResult{SceneTexture::create(
            device,
            allocator,
            textureCapacity,
            VK_FORMAT_R16G16B16A16_UNORM,
            VK_FORMAT_D32_SFLOAT
        )};
        sceneTextureResult.has_value())
    {
        layer.m_sceneTexture =
            std::make_unique<SceneTexture>(std::move(sceneTextureResult).value()
            );

        SceneTexture& sceneTexture{*layer.m_sceneTexture};

        layer.m_imguiSceneTextureHandle = ImGui_ImplVulkan_AddTexture(
            sceneTexture.colorSampler(),
            sceneTexture.color().view(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }
    else
    {
        SZG_ERROR("Failed to allocate UI Layer scene texture.");
        return std::nullopt;
    }

    layer.m_defaultPreferences = defaultPreferences;
    layer.m_currentPreferences = defaultPreferences;
    layer.m_device = device;

    uiReload(device, layer.m_currentPreferences);

    return layerResult;
}
auto UILayer::begin() -> syzygy::DockingLayout const&
{
    if (m_reloadNecessary)
    {
        uiReload(m_device, m_currentPreferences);

        m_reloadNecessary = false;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    m_open = true;

    m_currentHUD = renderHUD(m_currentPreferences);

    m_reloadNecessary = m_currentHUD.applyPreferencesRequested
                     || m_currentHUD.resetPreferencesRequested;
    if (m_currentHUD.resetPreferencesRequested)
    {
        m_currentPreferences = m_defaultPreferences;
    }

    m_currentDockingLayout = {};
    if (m_currentHUD.rebuildLayoutRequested && m_currentHUD.dockspaceID != 0)
    {
        m_currentDockingLayout = buildDefaultMultiWindowLayout(
            m_currentHUD.workArea, m_currentHUD.dockspaceID
        );
    }

    return m_currentDockingLayout;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto UILayer::HUDMenuItem(std::string const& menu, std::string const& item)
    const -> bool
{
    if (!m_open)
    {
        SZG_WARNING("UILayer method called while UI frame is not open.");
        return false;
    }

    bool clicked{};

    ImGui::Begin("BackgroundWindow");

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu(menu.c_str()))
        {
            clicked = ImGui::MenuItem(item.c_str());
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::End();

    return clicked;
}

auto UILayer::sceneTextureLayout() const -> std::optional<VkDescriptorSetLayout>
{
    if (m_sceneTexture == nullptr)
    {
        return std::nullopt;
    }

    return m_sceneTexture->singletonLayout();
}

auto UILayer::sceneViewport(bool const forceFocus)
    -> std::optional<SceneViewport>
{
    if (m_sceneTexture == nullptr)
    {
        SZG_WARNING("No scene texture to draw into.");
        return std::nullopt;
    }

    VkExtent2D const sceneTextureMax{m_sceneTexture->color().image().extent2D()
    };
    WindowResult<std::optional<VkRect2D>> widgetResult{sceneViewportWindow(
        "Scene Viewport",
        m_currentDockingLayout.centerTop,
        m_currentHUD.maximizeSceneViewport ? m_currentHUD.workArea
                                           : std::optional<UIRectangle>{},
        m_imguiSceneTextureHandle,
        ImVec2{
            static_cast<float>(sceneTextureMax.width),
            static_cast<float>(sceneTextureMax.height)
        },
        forceFocus
    )};

    if (!widgetResult.payload.has_value())
    {
        // Widget did not render any area, there is no viewport to render the
        // scene into
        return std::nullopt;
    }

    return SceneViewport{
        .focused = widgetResult.focused,
        .texture = *m_sceneTexture,
        .renderedSubregion = widgetResult.payload.value(),
    };
}

auto UILayer::sceneTexture() -> SceneTexture const& { return *m_sceneTexture; }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void UILayer::setCursorEnabled(bool const enabled, bool const breakWindowFocus)
{
    if (enabled)
    {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }
    else
    {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }

    if (breakWindowFocus)
    {
        ImGui::SetWindowFocus(nullptr);
    }
}

void UILayer::addWidget(std::unique_ptr<UIWidget> widget)
{
    m_activeWidgets.push_back(std::move(widget));
}

void UILayer::renderWidgets()
{
    m_activeWidgets.erase(
        std::remove_if(
            m_activeWidgets.begin(),
            m_activeWidgets.end(),
            [](std::unique_ptr<UIWidget> const& widget)
    { return widget == nullptr; }
        ),
        m_activeWidgets.end()
    );

    for (std::unique_ptr<UIWidget>& widget : m_activeWidgets)
    {
        if (widget->shouldClose())
        {
            widget->close();
            widget.reset();
        }
        else
        {
            widget->draw();
        }
    }
}

void UILayer::end()
{
    if (!m_open)
    {
        SZG_ERROR("UILayer::end() called without matching UILayer::open().");
        return;
    };

    ImGui::Render();

    m_open = false;
}
auto UILayer::recordDraw(VkCommandBuffer const cmd)
    -> std::optional<UIOutputImage>
{
    if (m_outputTexture == nullptr)
    {
        SZG_ERROR("UI Layer had no texture to render to.");
        return std::nullopt;
    }

    if (m_sceneTexture != nullptr)
    {
        m_sceneTexture->color().recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }

    m_outputTexture->color().recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    ImDrawData* const drawData{ImGui::GetDrawData()};

    // TODO: when is this offset nonzero?
    VkRect2D const renderedArea{
        .offset{VkOffset2D{
            .x = static_cast<int32_t>(drawData->DisplayPos.x),
            .y = static_cast<int32_t>(drawData->DisplayPos.y),
        }},
        .extent{VkExtent2D{
            .width = static_cast<uint32_t>(drawData->DisplaySize.x),
            .height = static_cast<uint32_t>(drawData->DisplaySize.y),
        }},
    };

    VkRenderingAttachmentInfo const colorAttachmentInfo{
        syzygy::renderingAttachmentInfo(
            m_outputTexture->color().view(),
            VK_IMAGE_LAYOUT_GENERAL,
            VkClearValue{
                .color = VkClearColorValue{.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}
            }
        )
    };
    std::vector<VkRenderingAttachmentInfo> const colorAttachments{
        colorAttachmentInfo
    };
    VkRenderingInfo const renderingInfo{
        syzygy::renderingInfo(renderedArea, colorAttachments, nullptr)
    };
    vkCmdBeginRendering(cmd, &renderingInfo);

    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

    vkCmdEndRendering(cmd);

    return UIOutputImage{
        .texture = *m_outputTexture,
        .renderedSubregion = renderedArea,
    };
}
} // namespace syzygy