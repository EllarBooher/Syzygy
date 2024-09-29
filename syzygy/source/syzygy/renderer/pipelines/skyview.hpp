#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/shaders.hpp"
#include <glm/vec2.hpp>
#include <memory>

namespace syzygy
{
struct Image;
struct AtmospherePacked;
struct CameraPacked;
template <typename T> struct TStagedBuffer;
} // namespace syzygy

namespace syzygy
{
struct SkyViewComputePipeline
{
public:
    auto operator=(SkyViewComputePipeline&&)
        -> SkyViewComputePipeline& = delete;
    SkyViewComputePipeline(SkyViewComputePipeline const&) = delete;
    auto operator=(SkyViewComputePipeline const&)
        -> SkyViewComputePipeline& = delete;

    SkyViewComputePipeline(SkyViewComputePipeline&&) noexcept;
    ~SkyViewComputePipeline();

    [[nodiscard]] static auto create(VkDevice device, VmaAllocator allocator)
        -> std::unique_ptr<SkyViewComputePipeline>;

    void recordDrawCommands(
        VkCommandBuffer cmd,
        VkRect2D drawRect,
        syzygy::Image& color,
        uint32_t atmosphereIndex,
        TStagedBuffer<syzygy::AtmospherePacked> const& atmospheres,
        uint32_t viewCameraIndex,
        TStagedBuffer<syzygy::CameraPacked> const& cameras
    );

    struct TransmittanceLUTResources
    {
        std::unique_ptr<syzygy::ImageView> map{};

        // Shader excerpt:
        // set = 0
        // binding = 0 -> image2D transmittance_LUT;
        VkDescriptorSet set{VK_NULL_HANDLE};
        VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
        VkPipelineLayout layout{VK_NULL_HANDLE};

        struct PushConstant
        {
            VkDeviceAddress atmosphereBuffer{};
            uint32_t atmosphereIndex{0};

            // NOLINTBEGIN(modernize-avoid-c-arrays, readability-magic-numbers)
            uint8_t padding[4]{0};
            // NOLINTEND(modernize-avoid-c-arrays, readability-magic-numbers)
        };
        ShaderObjectReflected shader{ShaderObjectReflected::makeInvalid()};
    };
    struct SkyViewLUTResources
    {
        std::unique_ptr<syzygy::ImageView> map{};

        // Shader excerpt:
        // set = 0
        // binding = 0 -> image2D skyview_LUT
        // binding = 1 -> sampler2D transmittance_LUT
        VkDescriptorSet set{VK_NULL_HANDLE};
        VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
        VkPipelineLayout layout{VK_NULL_HANDLE};

        VkSampler transmittanceImmutableSampler{VK_NULL_HANDLE};

        struct PushConstant
        {
            VkDeviceAddress atmosphereBuffer{};
            uint32_t atmosphereIndex{0};

            // NOLINTBEGIN(modernize-avoid-c-arrays, readability-magic-numbers)
            uint8_t padding[4]{0};
            // NOLINTEND(modernize-avoid-c-arrays, readability-magic-numbers)
        };
        ShaderObjectReflected shader{ShaderObjectReflected::makeInvalid()};
    };
    struct PerspectiveMapResources
    {
        std::unique_ptr<syzygy::ImageView> outputImage{};

        // Shader excerpt:
        // set = 0
        // binding = 0 -> image2D image
        // binding = 1 -> sampler2D azimuthElevationMap;

        VkDescriptorSet set{VK_NULL_HANDLE};
        VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
        VkPipelineLayout layout{VK_NULL_HANDLE};

        VkSampler azimuthElevationMapSampler{VK_NULL_HANDLE};

        struct PushConstant
        {
            VkDeviceAddress cameraBuffer{};

            uint32_t cameraIndex{0};
            // NOLINTBEGIN(modernize-avoid-c-arrays, readability-magic-numbers)
            uint8_t padding[4]{0};
            // NOLINTEND(modernize-avoid-c-arrays, readability-magic-numbers)

            glm::uvec2 drawExtent{};
        };
        ShaderObjectReflected shader{ShaderObjectReflected::makeInvalid()};
    };

private:
    SkyViewComputePipeline() = default;
    void destroy();

    bool m_hasAllocations{false};

    VkDevice m_device{VK_NULL_HANDLE};

    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator{};

    TransmittanceLUTResources m_transmittanceLUT{};
    SkyViewLUTResources m_skyViewLUT{};
    PerspectiveMapResources m_perspectiveMap{};
};
} // namespace syzygy