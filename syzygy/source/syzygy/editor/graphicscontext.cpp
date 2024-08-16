#include "graphicscontext.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/editor/window.hpp"
#include "syzygy/helpers.hpp"
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <optional>
#include <span>
#include <spdlog/fmt/bundled/core.h>
#include <utility>
#include <vector>

namespace
{
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
) -> std::optional<VmaAllocator>
{
    std::optional<VmaAllocator> allocatorResult{std::in_place};
    VmaAllocatorCreateInfo const allocatorInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = instance,
    };

    if (VkResult const createResult{
            vmaCreateAllocator(&allocatorInfo, &allocatorResult.value())
        };
        createResult != VK_SUCCESS)
    {
        return std::nullopt;
    }

    return allocatorResult;
}
} // namespace

GraphicsContext::GraphicsContext(GraphicsContext&& other) noexcept
{
    m_instance = std::exchange(other.m_instance, VK_NULL_HANDLE);
    m_debugMessenger = std::exchange(other.m_debugMessenger, VK_NULL_HANDLE);
    m_surface = std::exchange(other.m_surface, VK_NULL_HANDLE);
    m_physicalDevice = std::exchange(other.m_physicalDevice, VK_NULL_HANDLE);

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);

    m_universalQueue = std::exchange(other.m_universalQueue, VK_NULL_HANDLE);
    m_universalQueueFamily = std::exchange(other.m_universalQueueFamily, 0);

    m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
    m_descriptorAllocator = std::move(other.m_descriptorAllocator);
}

GraphicsContext::~GraphicsContext() { destroy(); }

auto GraphicsContext::create(PlatformWindow const& window)
    -> std::optional<GraphicsContext>
{
    std::optional<GraphicsContext> graphicsResult{
        std::in_place, GraphicsContext{}
    };
    GraphicsContext& graphics{graphicsResult.value()};

    if (volkInitialize() != VK_SUCCESS)
    {
        SZG_ERROR("Failed to initialize Volk.");
        return std::nullopt;
    }

    vkb::Result<vkb::Instance> const instanceBuildResult{
        vkb::InstanceBuilder{}
            .set_app_name("Syzygy")
            .request_validation_layers()
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .build()
    };
    if (!instanceBuildResult.has_value())
    {
        SZG_LOG_VKB(
            instanceBuildResult, "Failed to create VkBootstrap instance."
        );
        return std::nullopt;
    }
    vkb::Instance const& instance{instanceBuildResult.value()};
    // Load instance function pointers ASAP so destructors work
    volkLoadInstance(instance.instance);

    graphics.m_debugMessenger = instance.debug_messenger;
    graphics.m_instance = instance.instance;

    if (VkResult const surfaceResult{glfwCreateWindowSurface(
            instance.instance, window.handle(), nullptr, &graphics.m_surface
        )};
        surfaceResult != VK_SUCCESS)
    {
        SZG_LOG_VK(surfaceResult, "Failed to create surface via GLFW.");
        return std::nullopt;
    }

    vkb::Result<vkb::PhysicalDevice> physicalDeviceResult{
        selectPhysicalDevice(instance, graphics.m_surface)
    };
    if (!physicalDeviceResult.has_value())
    {
        SZG_LOG_VKB(physicalDeviceResult, "Failed to select physical device.");
        return std::nullopt;
    }
    vkb::PhysicalDevice const& physicalDevice{physicalDeviceResult.value()};
    graphics.m_physicalDevice = physicalDevice.physical_device;

    vkb::Result<vkb::Device> const deviceBuildResult{
        vkb::DeviceBuilder{physicalDevice}.build()
    };
    if (!deviceBuildResult.has_value())
    {
        SZG_LOG_VKB(deviceBuildResult, "Failed to build logical device.");
        return std::nullopt;
    }
    vkb::Device const& device{deviceBuildResult.value()};
    // Load device function pointers ASAP so destructors work
    volkLoadDevice(device.device);
    graphics.m_device = device.device;

    if (vkb::Result<VkQueue> const graphicsQueueResult{
            device.get_queue(vkb::QueueType::graphics)
        };
        graphicsQueueResult.has_value())
    {
        graphics.m_universalQueue = graphicsQueueResult.value();
    }
    else
    {
        SZG_LOG_VKB(graphicsQueueResult, "Failed to get graphics queue.");
        return std::nullopt;
    }

    if (vkb::Result<uint32_t> const graphicsQueueFamilyResult{
            device.get_queue_index(vkb::QueueType::graphics)
        };
        graphicsQueueFamilyResult.has_value())
    {
        graphics.m_universalQueueFamily = graphicsQueueFamilyResult.value();
    }
    else
    {
        SZG_LOG_VKB(
            graphicsQueueFamilyResult, "Failed to get graphics queue family."
        );
        return std::nullopt;
    }

    if (std::optional<VmaAllocator> const allocatorResult{createAllocator(
            graphics.m_physicalDevice, graphics.m_device, graphics.m_instance
        )};
        allocatorResult.has_value())
    {
        graphics.m_allocator = allocatorResult.value();
    }
    else
    {
        SZG_ERROR("Failed to create VMA Allocator.");
        return std::nullopt;
    }

    std::vector<syzygy::DescriptorAllocator::PoolSizeRatio> const poolSizes{
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0.5F},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0.5F}
    };

    uint32_t constexpr MAX_SETS{10U};

    if (std::optional<syzygy::DescriptorAllocator> descriptorAllocatorResult{
            syzygy::DescriptorAllocator::create(
                graphics.m_device,
                MAX_SETS,
                poolSizes,
                (VkDescriptorPoolCreateFlags)0
            )
        };
        descriptorAllocatorResult.has_value())
    {
        graphics.m_descriptorAllocator =
            std::make_unique<syzygy::DescriptorAllocator>(
                std::move(descriptorAllocatorResult).value()
            );
    }
    else
    {
        SZG_ERROR("Failed to create Descriptor Allocator.");
        return std::nullopt;
    }

    return graphicsResult;
}

// NOLINTNEXTLINE(readability-make-member-function-const)
auto GraphicsContext::instance() -> VkInstance { return m_instance; }

// NOLINTNEXTLINE(readability-make-member-function-const)
auto GraphicsContext::surface() -> VkSurfaceKHR { return m_surface; }

// NOLINTNEXTLINE(readability-make-member-function-const)
auto GraphicsContext::physicalDevice() -> VkPhysicalDevice
{
    return m_physicalDevice;
}

// NOLINTNEXTLINE(readability-make-member-function-const)
auto GraphicsContext::device() -> VkDevice { return m_device; }

// NOLINTNEXTLINE(readability-make-member-function-const)
auto GraphicsContext::universalQueue() -> VkQueue { return m_universalQueue; }

auto GraphicsContext::universalQueueFamily() const -> uint32_t
{
    return m_universalQueueFamily;
}

// NOLINTNEXTLINE(readability-make-member-function-const)
auto GraphicsContext::allocator() -> VmaAllocator { return m_allocator; }

auto GraphicsContext::descriptorAllocator() -> syzygy::DescriptorAllocator&
{
    return *m_descriptorAllocator;
}

void GraphicsContext::destroy()
{
    m_descriptorAllocator.reset();

    if (m_allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(m_allocator);
    }
    m_allocator = VK_NULL_HANDLE;

    m_universalQueue = VK_NULL_HANDLE;
    m_universalQueueFamily = 0;

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
    }
    m_device = VK_NULL_HANDLE;

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }
    else if (m_surface != VK_NULL_HANDLE || m_debugMessenger != VK_NULL_HANDLE)
    {
        SZG_WARNING("Surface and Debug Messenger were allocated while instance "
                    "was null. Memory was possibly leaked.");
    }

    m_instance = VK_NULL_HANDLE;
    m_debugMessenger = VK_NULL_HANDLE;
    m_surface = VK_NULL_HANDLE;
}
