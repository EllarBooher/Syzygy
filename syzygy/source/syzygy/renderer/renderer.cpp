#include "renderer.hpp"

#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageoperations.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include "syzygy/renderer/pipelines/deferred.hpp"
#include "syzygy/renderer/scene.hpp"
#include "syzygy/renderer/scenetexture.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include "syzygy/ui/dockinglayout.hpp"
#include "syzygy/ui/engineui.hpp"
#include "syzygy/ui/pipelineui.hpp"
#include "syzygy/ui/uiwindow.hpp"
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace syzygy
{
Renderer::Renderer(Renderer&& other) noexcept
{
    destroy();

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
    m_initialized = std::exchange(other.m_initialized, false);

    m_sceneDepthTexture = std::move(other.m_sceneDepthTexture);

    m_debugLines = std::exchange(other.m_debugLines, {});

    m_activeRenderingPipeline = std::exchange(
        other.m_activeRenderingPipeline, RenderingPipelines::DEFERRED
    );

    m_genericComputePipeline = std::move(other.m_genericComputePipeline);
    m_deferredShadingPipeline = std::move(other.m_deferredShadingPipeline);

    m_camerasBuffer = std::move(other.m_camerasBuffer);
    m_atmospheresBuffer = std::move(other.m_atmospheresBuffer);
}

Renderer::~Renderer() { destroy(); }

void Renderer::destroy()
{
    if (m_device == VK_NULL_HANDLE || m_allocator == VK_NULL_HANDLE)
    {
        if (m_initialized)
        {
            SZG_WARNING("Initialized renderer being destroyed had NULL device "
                        "and/or allocator handles.");
        }
        return;
    }

    m_sceneDepthTexture.reset();

    m_debugLines.cleanup(m_device, m_allocator);
    m_debugLines = {};

    m_activeRenderingPipeline = RenderingPipelines::DEFERRED;
    m_genericComputePipeline->cleanup(m_device);
    m_deferredShadingPipeline->cleanup(m_device, m_allocator);

    m_camerasBuffer.reset();
    m_atmospheresBuffer.reset();

    m_device = VK_NULL_HANDLE;
    m_allocator = VK_NULL_HANDLE;

    m_initialized = false;
}

auto Renderer::create(
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator,
    VkDescriptorSetLayout const computeImageDescriptorLayout
) -> std::optional<Renderer>
{
    std::optional<Renderer> rendererResult{Renderer{}};
    Renderer& renderer{rendererResult.value()};
    renderer.m_device = device;
    renderer.m_allocator = allocator;
    renderer.m_initialized = true;

    renderer.initDrawTargets(device, allocator);

    renderer.initWorld(device, allocator);
    renderer.initDebug(device, allocator);
    renderer.initGenericComputePipelines(device, computeImageDescriptorLayout);

    renderer.initDeferredShadingPipeline(
        device, allocator, descriptorAllocator
    );

    return rendererResult;
}

void Renderer::initDrawTargets(
    VkDevice const device, VmaAllocator const allocator
)
{
    // Initialize the image used for rendering outside of the swapchain.

    VkExtent2D constexpr RESERVED_IMAGE_EXTENT{
        MAX_DRAW_EXTENTS.width, MAX_DRAW_EXTENTS.height
    };

    if (std::optional<std::unique_ptr<ImageView>> sceneDepthResult{
            ImageView::allocate(
                device,
                allocator,
                ImageAllocationParameters{
                    .extent = RESERVED_IMAGE_EXTENT,
                    .format = VK_FORMAT_D32_SFLOAT,
                    .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                | VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                },
                ImageViewAllocationParameters{
                    .subresourceRange =
                        imageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)
                }
            )
        };
        sceneDepthResult.has_value())
    {
        m_sceneDepthTexture = std::move(sceneDepthResult).value();
    }
    else
    {
        SZG_WARNING("Failed to allocate scene depth texture.");
    }
}

void Renderer::initWorld(VkDevice const device, VmaAllocator const allocator)
{
    m_camerasBuffer = std::make_unique<TStagedBuffer<CameraPacked>>(
        TStagedBuffer<CameraPacked>::allocate(
            device,
            allocator,
            CAMERA_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        )
    );
    m_atmospheresBuffer = std::make_unique<TStagedBuffer<AtmospherePacked>>(
        TStagedBuffer<AtmospherePacked>::allocate(
            device,
            allocator,
            ATMOSPHERE_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        )
    );
}

void Renderer::initDebug(VkDevice const device, VmaAllocator const allocator)
{
    m_debugLines.pipeline = std::make_unique<DebugLineGraphicsPipeline>(
        device,
        DebugLineGraphicsPipeline::ImageFormats{
            .color = VK_FORMAT_R16G16B16A16_SFLOAT,
            .depth = m_sceneDepthTexture->image().format(),
        }
    );
    m_debugLines.indices = std::make_unique<TStagedBuffer<uint32_t>>(
        TStagedBuffer<uint32_t>::allocate(
            device,
            allocator,
            DEBUGLINES_CAPACITY,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        )
    );
    m_debugLines.vertices = std::make_unique<TStagedBuffer<VertexPacked>>(
        TStagedBuffer<VertexPacked>::allocate(
            device,
            allocator,
            DEBUGLINES_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        )
    );
}

void Renderer::initDeferredShadingPipeline(
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator
)
{
    m_deferredShadingPipeline = std::make_unique<DeferredShadingPipeline>(
        device, allocator, descriptorAllocator, MAX_DRAW_EXTENTS
    );

    m_deferredShadingPipeline->updateRenderTargetDescriptors(
        device, *m_sceneDepthTexture
    );
}

void Renderer::initGenericComputePipelines(
    VkDevice const device, VkDescriptorSetLayout const imageDescriptorLayout
)
{
    std::vector<std::string> const shaderPaths{
        "shaders/booleanpush.comp.spv",
        "shaders/gradient_color.comp.spv",
        "shaders/sparse_push_constant.comp.spv",
        "shaders/matrix_color.comp.spv"
    };
    m_genericComputePipeline = std::make_unique<ComputeCollectionPipeline>(
        device, imageDescriptorLayout, shaderPaths
    );
}

void Renderer::uiEngineControls(DockingLayout const& dockingLayout)
{
    if (UIWindow const engineControls{
            UIWindow::beginDockable("Engine Controls", dockingLayout.right)
        };
        engineControls.isOpen())
    {
        imguiRenderingSelection(m_activeRenderingPipeline);

        ImGui::Separator();
        switch (m_activeRenderingPipeline)
        {
        case RenderingPipelines::DEFERRED:
            imguiPipelineControls(*m_deferredShadingPipeline);
            break;
        case RenderingPipelines::COMPUTE_COLLECTION:
            imguiPipelineControls(*m_genericComputePipeline);
            break;
        default:
            ImGui::Text("Invalid rendering pipeline selected.");
            break;
        }

        ImGui::Separator();
        imguiStructureControls(m_debugLines);
    }
}

void Renderer::recordDraw(
    VkCommandBuffer const cmd,
    Scene const& scene,
    SceneTexture& sceneTexture,
    VkRect2D const sceneSubregion
)
{
    // Begin syzygy drawing

    m_debugLines.clear();
    if (sceneSubregion.extent.width <= 0 || sceneSubregion.extent.height <= 0)
    {
        return;
    }

    std::optional<double> const viewportAspectRatioResult{
        aspectRatio(sceneSubregion.extent)
    };
    if (!viewportAspectRatioResult.has_value())
    {
        return;
    }
    double const aspectRatio{viewportAspectRatioResult.value()};

    { // Copy cameras to gpu
        CameraPacked const mainCamera{
            scene.camera.toDeviceEquivalent(static_cast<float>(aspectRatio))
        };

        m_camerasBuffer->clearStaged();
        m_camerasBuffer->push(mainCamera);
        m_camerasBuffer->recordCopyToDevice(cmd);
    }

    std::vector<DirectionalLightPacked> directionalLights{};
    { // Copy atmospheres to gpu
        AtmosphereBaked const bakedAtmosphere{
            scene.atmosphere.baked(scene.bounds)
        };
        if (bakedAtmosphere.moonlight.has_value())
        {
            directionalLights.push_back(bakedAtmosphere.moonlight.value());
        }
        if (bakedAtmosphere.sunlight.has_value())
        {
            directionalLights.push_back(bakedAtmosphere.sunlight.value());
        }

        m_atmospheresBuffer->clearStaged();
        m_atmospheresBuffer->push(bakedAtmosphere.atmosphere);
        m_atmospheresBuffer->recordCopyToDevice(cmd);
    }

    for (MeshInstanced const& instance : scene.geometry)
    {
        if (instance.models != nullptr)
        {
            instance.models->recordCopyToDevice(cmd);
        }
        if (instance.models != nullptr)
        {
            instance.modelInverseTransposes->recordCopyToDevice(cmd);
        }
    }

    {
        sceneTexture.texture().recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_GENERAL
        );

        switch (m_activeRenderingPipeline)
        {
        case RenderingPipelines::DEFERRED:
        {
            // TODO: create a struct that contains a ref to a struct in a
            // buffer
            uint32_t const cameraIndex{0};
            uint32_t const atmosphereIndex{0};

            m_deferredShadingPipeline->recordDrawCommands(
                cmd,
                sceneSubregion,
                sceneTexture.texture().image(),
                *m_sceneDepthTexture,
                directionalLights,
                scene.spotlightsRender ? scene.spotlights
                                       : std::vector<SpotLightPacked>{},
                cameraIndex,
                *m_camerasBuffer,
                atmosphereIndex,
                *m_atmospheresBuffer,
                scene.geometry
            );

            sceneTexture.texture().recordTransitionBarriered(
                cmd, VK_IMAGE_LAYOUT_GENERAL
            );

            m_debugLines.pushBox(
                scene.bounds.center,
                glm::identity<glm::quat>(),
                scene.bounds.extent
            );

            recordDrawDebugLines(
                cmd, cameraIndex, sceneTexture, sceneSubregion, *m_camerasBuffer
            );

            break;
        }
        case RenderingPipelines::COMPUTE_COLLECTION:
        {
            m_genericComputePipeline->recordDrawCommands(
                cmd, sceneTexture.singletonDescriptor(), sceneSubregion.extent
            );

            break;
        }
        }
    }

    // End syzygy drawing
}

void Renderer::recordDrawDebugLines(
    VkCommandBuffer const cmd,
    uint32_t const cameraIndex,
    SceneTexture& sceneTexture,
    VkRect2D const sceneSubregion,
    TStagedBuffer<CameraPacked> const& camerasBuffer
)
{
    m_debugLines.lastFrameDrawResults = {};

    if (m_debugLines.enabled && m_debugLines.indices->stagedSize() > 0)
    {
        m_debugLines.recordCopy(cmd);

        DrawResultsGraphics const drawResults{
            m_debugLines.pipeline->recordDrawCommands(
                cmd,
                false,
                m_debugLines.lineWidth,
                sceneSubregion,
                sceneTexture.texture(),
                *m_sceneDepthTexture,
                cameraIndex,
                camerasBuffer,
                *m_debugLines.vertices,
                *m_debugLines.indices
            )
        };

        m_debugLines.lastFrameDrawResults = drawResults;
    }
}
} // namespace syzygy