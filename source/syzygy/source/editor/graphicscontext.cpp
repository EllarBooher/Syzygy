#include "graphicscontext.hpp"

#include "../core/deletionqueue.hpp"
#include "../helpers.hpp"
#include <GLFW/glfw3.h>
#include <optional>

template <typename T> struct Result
{
public:
    [[nodiscard]] auto value() const -> T const&
    {
        assert(has_value());
        return m_value.value(); // NOLINT(bugprone-unchecked-optional-access)
    }

    [[nodiscard]] auto has_value() const -> bool { return m_value.has_value(); }

    [[nodiscard]] auto vk_result() const -> VkResult { return m_result; }

private:
    Result(T const& value, VkResult result)
        : m_value(value)
        , m_result(result)
    {
    }
    Result(T&& value, VkResult result)
        : m_value(std::move(value))
        , m_result(result)
    {
    }

    Result(VkResult result)
        : m_value(std::nullopt)
        , m_result(result)
    {
    }

public:
    static auto make_value(T const& value, VkResult result) -> Result
    {
        return Result{value, result};
    }
    static auto make_value(T&& value, VkResult result) -> Result
    {
        return Result{std::move(value), result};
    }

    static auto make_empty(VkResult result) -> Result { return Result{result}; }

private:
    std::optional<T> m_value;
    VkResult m_result;
};

namespace
{
auto buildInstance() -> vkb::Result<vkb::Instance>
{
    return vkb::InstanceBuilder{}
        .set_app_name("Renderer")
        .request_validation_layers()
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();
}

auto createSurface(VkInstance const instance, GLFWwindow* const window)
    -> Result<VkSurfaceKHR>
{
    VkSurfaceKHR surface{};
    VkResult const surfaceBuildResult{
        glfwCreateWindowSurface(instance, window, nullptr, &surface)
    };
    if (surfaceBuildResult != VK_SUCCESS)
    {
        return Result<VkSurfaceKHR>::make_empty(surfaceBuildResult);
    }

    return Result<VkSurfaceKHR>::make_value(surface, surfaceBuildResult);
}

auto selectPhysicalDevice(
    vkb::Instance const& instance, VkSurfaceKHR const surface
) -> vkb::Result<vkb::PhysicalDevice>
{
    VkPhysicalDeviceVulkan13Features const features13{
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features const features12{
        .descriptorIndexing = VK_TRUE,

        .descriptorBindingPartiallyBound = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,

        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceFeatures const features{
        .wideLines = VK_TRUE,
    };

    VkPhysicalDeviceShaderObjectFeaturesEXT const shaderObjectFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
        .pNext = nullptr,

        .shaderObject = VK_TRUE,
    };

    return vkb::PhysicalDeviceSelector{instance}
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_required_features(features)
        .add_required_extension_features(shaderObjectFeature)
        .add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME)
        .set_surface(surface)
        .select();
}

auto createAllocator(
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VkInstance const instance
) -> Result<VmaAllocator>
{
    VmaAllocator allocator{};
    VmaAllocatorCreateInfo const allocatorInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = instance,
    };
    VkResult const createResult{vmaCreateAllocator(&allocatorInfo, &allocator)};

    if (createResult != VK_SUCCESS)
    {
        return Result<VmaAllocator>::make_empty(createResult);
    }

    return Result<VmaAllocator>::make_value(allocator, createResult);
}
} // namespace

auto VulkanContext::create(GLFWwindow* window) -> std::optional<VulkanContext>
{
    DeletionQueue cleanupCallbacks{};

    vkb::Result<vkb::Instance> const instanceBuildResult{buildInstance()};
    if (!instanceBuildResult.has_value())
    {
        LogVkbError(
            instanceBuildResult, "Failed to create VkBootstrap instance."
        );
        cleanupCallbacks.flush();
        return std::nullopt;
    }
    vkb::Instance const instance{instanceBuildResult.value()};
    cleanupCallbacks.pushFunction([&]() { vkb::destroy_instance(instance); });

    Result<VkSurfaceKHR> const surfaceResult{
        createSurface(instance.instance, window)
    };
    if (!surfaceResult.has_value())
    {
        Error(fmt::format(
            "Failed to create surface via GLFW. Error: {}",
            string_VkResult(surfaceResult.vk_result())
        ));
        cleanupCallbacks.flush();
        return std::nullopt;
    }
    VkSurfaceKHR const surface{surfaceResult.value()};
    cleanupCallbacks.pushFunction([&]()
    { vkb::destroy_surface(instance, surface); });

    vkb::Result<vkb::PhysicalDevice> physicalDeviceResult{
        selectPhysicalDevice(instance, surface)
    };
    if (!physicalDeviceResult.has_value())
    {
        LogVkbError(physicalDeviceResult, "Failed to select physical device.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }
    vkb::PhysicalDevice const physicalDevice{physicalDeviceResult.value()};

    vkb::Result<vkb::Device> const deviceBuildResult{
        vkb::DeviceBuilder{physicalDevice}.build()
    };
    if (!deviceBuildResult.has_value())
    {
        LogVkbError(deviceBuildResult, "Failed to build logical device.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }
    vkb::Device const& device{deviceBuildResult.value()};
    cleanupCallbacks.pushFunction([&]() { vkb::destroy_device(device); });

    vkb::Result<VkQueue> const graphicsQueueResult{
        device.get_queue(vkb::QueueType::graphics)
    };
    if (!graphicsQueueResult.has_value())
    {
        LogVkbError(graphicsQueueResult, "Failed to get graphics queue.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }
    VkQueue const graphicsQueue{graphicsQueueResult.value()};

    vkb::Result<uint32_t> const graphicsQueueFamilyResult{
        device.get_queue_index(vkb::QueueType::graphics)
    };
    if (!graphicsQueueFamilyResult.has_value())
    {
        LogVkbError(
            graphicsQueueFamilyResult, "Failed to get graphics queue family."
        );
        cleanupCallbacks.flush();
        return std::nullopt;
    }
    uint32_t const graphicsQueueFamily{graphicsQueueFamilyResult.value()};

    return VulkanContext{
        .instance = instance.instance,
        .debugMessenger = instance.debug_messenger,
        .surface = surface,
        .physicalDevice = device.physical_device,
        .device = device.device,
        .graphicsQueue = graphicsQueue,
        .graphicsQueueFamily = graphicsQueueFamily,
    };
}
void VulkanContext::destroy() const
{
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    vkDestroyInstance(instance, nullptr);
}

auto Swapchain::create(
    glm::u16vec2 const extent,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VkSurfaceKHR const surface
) -> std::optional<Swapchain>
{
    DeletionQueue cleanupCallbacks{};

    VkFormat constexpr SWAPCHAIN_IMAGE_FORMAT{VK_FORMAT_B8G8R8A8_UNORM};

    VkSurfaceFormatKHR const surfaceFormat{
        .format = SWAPCHAIN_IMAGE_FORMAT,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };

    uint32_t const width{extent.x};
    uint32_t const height{extent.y};

    vkb::Result<vkb::Swapchain> const swapchainResult{
        vkb::SwapchainBuilder{physicalDevice, device, surface}
            .set_desired_format(surfaceFormat)
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
    };
    if (!swapchainResult.has_value())
    {
        LogVkbError(swapchainResult, "Failed to build VkbSwapchain.");
        return std::nullopt;
    }
    vkb::Swapchain swapchain{swapchainResult.value()};
    cleanupCallbacks.pushFunction([&]() { vkb::destroy_swapchain(swapchain); });

    vkb::Result<std::vector<VkImage>> imagesResult{swapchain.get_images()};
    if (!imagesResult.has_value())
    {
        LogVkbError(imagesResult, "Failed to get swapchain images.");
        return std::nullopt;
    }

    vkb::Result<std::vector<VkImageView>> viewsResult{swapchain.get_image_views(
    )};
    if (!viewsResult.has_value())
    {
        LogVkbError(viewsResult, "Failed to get swapchain image views.");
        return std::nullopt;
    }
    std::vector<VkImageView> const views{viewsResult.value()};
    cleanupCallbacks.pushFunction([&]()
    { swapchain.destroy_image_views(views); });

    return Swapchain{
        .swapchain = swapchain.swapchain,
        .imageFormat = SWAPCHAIN_IMAGE_FORMAT,
        .images = imagesResult.value(),
        .imageViews = views,
        .extent = swapchain.extent,
    };
}

void Swapchain::destroy(VkDevice const device)
{
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    for (VkImageView const view : imageViews)
    {
        vkDestroyImageView(device, view, nullptr);
    }
}

auto GraphicsContext::create(PlatformWindow const& window)
    -> std::optional<GraphicsContext>
{
    DeletionQueue cleanupCallbacks{};

    volkInitialize();

    std::optional<VulkanContext> const vulkanResult{
        VulkanContext::create(window.handle)
    };
    if (!vulkanResult.has_value())
    {
        Error("Failed to create vulkan context.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }
    VulkanContext vulkanContext{vulkanResult.value()};
    cleanupCallbacks.pushFunction([&]() { vulkanContext.destroy(); });

    volkLoadInstance(vulkanContext.instance);
    volkLoadDevice(vulkanContext.device);

    Result<VmaAllocator> const allocatorResult{createAllocator(
        vulkanContext.physicalDevice,
        vulkanContext.device,
        vulkanContext.instance
    )};
    if (!allocatorResult.has_value())
    {
        Error(fmt::format(
            "Failed to create VMA allocator. Error: {}",
            string_VkResult(allocatorResult.vk_result())
        ));
        cleanupCallbacks.flush();
        return std::nullopt;
    }
    VmaAllocator const allocator{allocatorResult.value()};
    cleanupCallbacks.pushFunction([&]() { vmaDestroyAllocator(allocator); });

    std::optional<Swapchain> swapchainResult{Swapchain::create(
        window.extent(),
        vulkanContext.physicalDevice,
        vulkanContext.device,
        vulkanContext.surface
    )};
    if (!swapchainResult.has_value())
    {
        Error("Failed to create Swapchain.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }
    Swapchain swapchain{swapchainResult.value()};
    cleanupCallbacks.pushFunction([&]()
    { swapchain.destroy(vulkanContext.device); });

    return GraphicsContext{vulkanContext, allocator, swapchainResult.value()};
}

void GraphicsContext::destroy()
{
    Log("Cleaning up graphics context...");

    m_swapchain.destroy(m_vulkan.device);
    vmaDestroyAllocator(m_allocator);
    m_vulkan.destroy();

    Log("Graphics context cleaned up.");
}
