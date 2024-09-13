#include "swapchain.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include <algorithm>
#include <filesystem>
#include <optional>
#include <tuple>
#include <utility>

namespace syzygy
{
auto Swapchain::operator=(Swapchain&& other) noexcept -> Swapchain&
{
    destroy();

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_swapchain = std::exchange(other.m_swapchain, VK_NULL_HANDLE);
    m_imageFormat = std::exchange(other.m_imageFormat, VK_FORMAT_UNDEFINED);
    m_images = std::move(other.m_images);
    m_imageViews = std::move(other.m_imageViews);
    m_extent = std::exchange(other.m_extent, VkExtent2D{});

    m_descriptorAllocator = std::move(other.m_descriptorAllocator);

    m_imageSingletonDescriptors = std::move(other.m_imageSingletonDescriptors);
    m_imageSingletonLayout =
        std::exchange(other.m_imageSingletonLayout, VK_NULL_HANDLE);

    m_oetfPipelines = std::move(other.m_oetfPipelines);
    m_oetfPipelineLayout =
        std::exchange(other.m_oetfPipelineLayout, VK_NULL_HANDLE);

    return *this;
}
Swapchain::Swapchain(Swapchain&& other) noexcept { *this = std::move(other); }

Swapchain::~Swapchain() { destroy(); }

void Swapchain::destroy()
{
    if (m_device == VK_NULL_HANDLE)
    {
        // Check just one handle, since the lifetime of all members is the same
        if (m_swapchain != VK_NULL_HANDLE)
        {
            SZG_WARNING("Swapchain had allocations, but device was null. "
                        "Memory was possibly leaked.");
        }
        return;
    }

    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_imageSingletonLayout, nullptr);

    for (VkImageView const view : m_imageViews)
    {
        vkDestroyImageView(m_device, view, nullptr);
    }

    for (auto const& pipeline : m_oetfPipelines)
    {
        vkDestroyShaderEXT(m_device, pipeline.shaderObject(), nullptr);
    }
    vkDestroyPipelineLayout(m_device, m_oetfPipelineLayout, nullptr);
}
} // namespace syzygy

namespace
{
auto getBestFormat(
    VkPhysicalDevice const physicalDevice, VkSurfaceKHR const surface
) -> std::optional<VkSurfaceFormatKHR>
{
    uint32_t formatCount;
    SZG_TRY_VK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &formatCount, nullptr
        ),
        "Failed to query surface format support.",
        std::nullopt
    );

    std::vector<VkSurfaceFormatKHR> supportedFormats{formatCount};

    SZG_TRY_VK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &formatCount, supportedFormats.data()
        ),
        "Failed to query surface format support.",
        std::nullopt
    );

    std::optional<VkSurfaceFormatKHR> bestFormat{};

    std::vector<VkFormat> const formatPreferenceOrder{
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM
    };
    std::optional<size_t> bestFormatIndex{};

    for (auto const& format : supportedFormats)
    {
        if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            continue;
        }

        auto const formatIt = std::find(
            formatPreferenceOrder.begin(),
            formatPreferenceOrder.end(),
            format.format
        );

        if (formatIt == formatPreferenceOrder.end())
        {
            continue;
        }

        size_t const formatIndex{static_cast<size_t>(
            std::distance(formatPreferenceOrder.begin(), formatIt)
        )};

        // The format is supported, now we compare to the best we've found.

        if (!bestFormatIndex.has_value()
            || formatIndex < bestFormatIndex.value())
        {
            bestFormat = format;
            bestFormatIndex = formatIndex;
        }
    };

    return bestFormat;
}
} // namespace

namespace syzygy
{
auto Swapchain::create(
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VkSurfaceKHR const surface,
    glm::u16vec2 const extent,
    std::optional<VkSwapchainKHR> const old
) -> std::optional<Swapchain>
{
    if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE
        || surface == VK_NULL_HANDLE)
    {
        SZG_ERROR("One or more necessary handles were null.");
        return std::nullopt;
    }

    std::optional<Swapchain> swapchainResult{std::in_place, Swapchain{}};
    Swapchain& swapchain{swapchainResult.value()};
    swapchain.m_device = device;

    {
        std::vector<syzygy::DescriptorAllocator::PoolSizeRatio> const poolSizes{
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0F}
        };

        uint32_t constexpr MAX_SETS{10U};

        if (std::optional<syzygy::DescriptorAllocator>
                descriptorAllocatorResult{syzygy::DescriptorAllocator::create(
                    device, MAX_SETS, poolSizes, (VkDescriptorPoolCreateFlags)0
                )};
            descriptorAllocatorResult.has_value())
        {
            swapchain.m_descriptorAllocator =
                std::make_unique<syzygy::DescriptorAllocator>(
                    std::move(descriptorAllocatorResult).value()
                );
        }
        else
        {
            SZG_ERROR("Failed to create swapchain descriptor allocator.");
            return std::nullopt;
        }
    }

    std::optional<VkSurfaceFormatKHR> const surfaceFormatResult{
        getBestFormat(physicalDevice, surface)
    };
    if (!surfaceFormatResult.has_value())
    {
        SZG_ERROR("Could not find support for a suitable surface format.");
        return std::nullopt;
    }
    VkSurfaceFormatKHR const& surfaceFormat{surfaceFormatResult.value()};

    SZG_INFO(
        "Surface Format selected: Format: {}, ColorSpace: {}",
        string_VkFormat(surfaceFormat.format),
        string_VkColorSpaceKHR(surfaceFormat.colorSpace)
    );

    uint32_t const width{extent.x};
    uint32_t const height{extent.y};
    VkExtent2D const swapchainExtent{.width = width, .height = height};

    VkSwapchainCreateInfoKHR const swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,

        .flags = 0,
        .surface = surface,

        .minImageCount = 3,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                    | VK_IMAGE_USAGE_STORAGE_BIT
                    | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,

        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = 1,
        .oldSwapchain = old.value_or(VK_NULL_HANDLE),
    };

    if (VkResult const swapchainResult{vkCreateSwapchainKHR(
            device, &swapchainCreateInfo, nullptr, &swapchain.m_swapchain
        )})
    {
        SZG_ERROR("Failed to create swapchain.");
        return std::nullopt;
    }

    swapchain.m_imageFormat = surfaceFormat.format;
    swapchain.m_extent = swapchainExtent;

    uint32_t swapchainImageCount{0};
    if (vkGetSwapchainImagesKHR(
            device, swapchain.m_swapchain, &swapchainImageCount, nullptr
        ) != VK_SUCCESS
        || swapchainImageCount == 0)
    {
        SZG_ERROR("Failed to get swapchain images.");
        return std::nullopt;
    }

    swapchain.m_images.resize(swapchainImageCount);
    if (vkGetSwapchainImagesKHR(
            device,
            swapchain.m_swapchain,
            &swapchainImageCount,
            swapchain.m_images.data()
        )
        != VK_SUCCESS)
    {
        SZG_ERROR("Failed to get swapchain images.");
        return std::nullopt;
    }

    for (size_t index{0}; index < swapchain.m_images.size(); index++)
    {
        VkImage const image{swapchain.m_images[index]};

        VkImageViewCreateInfo const viewInfo{syzygy::imageViewCreateInfo(
            swapchain.m_imageFormat, image, VK_IMAGE_ASPECT_COLOR_BIT
        )};

        VkImageView view;
        if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS)
        {
            SZG_ERROR("Failed to create swapchain image view.");
            return std::nullopt;
        }
        swapchain.m_imageViews.push_back(view);
    }

    {
        if (auto const layoutResult{
                DescriptorLayoutBuilder{}
                    .addBinding(
                        DescriptorLayoutBuilder::AddBindingParameters{
                            .binding = 0,
                            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                            .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                            .bindingFlags = 0,
                        },
                        1
                    )
                    .build(device, 0)
            };
            layoutResult.has_value())
        {
            swapchain.m_imageSingletonLayout = layoutResult.value();
        }
        else
        {
            SZG_ERROR("Failed to allocate swapchain nonlinear conversion "
                      "descriptor layout.");
            return std::nullopt;
        }

        for (size_t swapchainIndex{0};
             swapchainIndex < swapchain.m_images.size();
             swapchainIndex++)
        {
            VkImageView const view{swapchain.m_imageViews[swapchainIndex]};

            VkDescriptorSet const singletonSet =
                swapchain.m_descriptorAllocator->allocate(
                    device, swapchain.m_imageSingletonLayout
                );
            swapchain.m_imageSingletonDescriptors.push_back(singletonSet);

            VkDescriptorImageInfo const imageInfo{
                .sampler = VK_NULL_HANDLE,
                .imageView = view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            };

            VkWriteDescriptorSet const writeInfo{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,

                .dstSet = singletonSet,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,

                .pImageInfo = &imageInfo,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr,
            };

            std::vector<VkWriteDescriptorSet> const writes{writeInfo};

            vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);

            swapchain.m_imageSingletonDescriptors.push_back(singletonSet);
        }

        std::vector<VkDescriptorSetLayout> const descriptorLayouts{
            swapchain.m_imageSingletonLayout
        };
        std::vector<std::tuple<GammaTransferFunction, char const*>> const
            oetfPaths{
                {GammaTransferFunction::PureGamma,
                 "shaders/transfer/oetf_pure_gamma.comp.spv"},
                {GammaTransferFunction::sRGB,
                 "shaders/transfer/oetf_srgb.comp.spv"},
            };

        for (auto const& [transferFunction, path] : oetfPaths)
        {
            std::optional<ShaderObjectReflected> shaderLoadResult{
                loadShaderObject(
                    device,
                    path,
                    VK_SHADER_STAGE_COMPUTE_BIT,
                    0,
                    descriptorLayouts,
                    {}
                )
            };

            if (!shaderLoadResult.has_value())
            {
                SZG_ERROR("Failed to create swapchain nonlinear conversion "
                          "shader object.");
                return std::nullopt;
            }

            swapchain.m_oetfPipelines[static_cast<size_t>(transferFunction)] =
                shaderLoadResult.value();
        }

        VkPipelineLayoutCreateInfo const layoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,

            .flags = 0,

            .setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size()),
            .pSetLayouts = descriptorLayouts.data(),

            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        };

        SZG_TRY_VK(
            vkCreatePipelineLayout(
                device,
                &layoutCreateInfo,
                nullptr,
                &swapchain.m_oetfPipelineLayout
            ),
            "Failed to create swapchain nonlinear conversion pipeline layout.",
            std::nullopt
        );
    }

    return swapchainResult;
}

auto Swapchain::swapchain() const -> VkSwapchainKHR { return m_swapchain; }

auto Swapchain::images() const -> std::span<VkImage const> { return m_images; }

auto Swapchain::imageViews() const -> std::span<VkImageView const>
{
    return m_imageViews;
}

auto Swapchain::extent() const -> VkExtent2D { return m_extent; }
auto Swapchain::imageDescriptors() const -> std::span<VkDescriptorSet const>
{
    return m_imageSingletonDescriptors;
}
auto Swapchain::eotfPipeline(GammaTransferFunction const transferFunction) const
    -> ShaderObjectReflected const&
{
    return m_oetfPipelines[static_cast<size_t>(transferFunction)];
}
auto Swapchain::eotfPipelineLayout() const -> VkPipelineLayout
{
    return m_oetfPipelineLayout;
}
} // namespace syzygy