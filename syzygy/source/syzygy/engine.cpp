#include "engine.hpp"

#include "syzygy/core/scene.hpp"
#include "syzygy/core/scenetexture.hpp"
#include "syzygy/debuglines.hpp"
#include "syzygy/enginetypes.hpp"
#include "syzygy/gputypes.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/images/image.hpp"
#include "syzygy/images/imageoperations.hpp"
#include "syzygy/images/imageview.hpp"
#include "syzygy/initializers.hpp"
#include "syzygy/pipelines.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/pipelines/deferred.hpp"
#include "syzygy/ui/dockinglayout.hpp"
#include "syzygy/ui/engineui.hpp"
#include "syzygy/ui/uiwindow.hpp"
#include "syzygy/vulkanusage.hpp"
#include "ui/pipelineui.hpp"
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <span>
#include <string>
#include <utility>
#include <vector>

#define VKRENDERER_COMPILE_WITH_TESTING 0

class DescriptorAllocator;
struct PlatformWindow;

Engine::Engine(
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator,
    scene::SceneTexture const& scene
)
{
    SZG_INFO("Initializing Engine...");

    initDrawTargets(device, allocator);

    initWorld(device, allocator);
    initDebug(device, allocator);
    initGenericComputePipelines(device, scene);

    initDeferredShadingPipeline(device, allocator, descriptorAllocator);

    SZG_INFO("Vulkan Initialized.");

    m_initialized = true;

    SZG_INFO("Engine Initialized.");
}

auto Engine::loadEngine(
    PlatformWindow const& /*window*/,
    VkInstance const /*instance*/,
    VkPhysicalDevice const /*physicalDevice*/,
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator,
    scene::SceneTexture const& sceneTexture,
    VkQueue const /*generalQueue*/,
    uint32_t const /*generalQueueFamilyIndex*/
) -> Engine*
{
    if (m_loadedEngine == nullptr)
    {
        SZG_INFO("Loading Engine.");
        m_loadedEngine =
            new Engine(device, allocator, descriptorAllocator, sceneTexture);
    }
    else
    {
        SZG_WARNING(
            "Called loadEngine when one was already loaded. No new engine "
            "was loaded."
        );
    }

    return m_loadedEngine;
}

void Engine::initDrawTargets(
    VkDevice const device, VmaAllocator const allocator
)
{
    // Initialize the image used for rendering outside of the swapchain.

    VkExtent2D constexpr RESERVED_IMAGE_EXTENT{
        MAX_DRAW_EXTENTS.width, MAX_DRAW_EXTENTS.height
    };

    if (std::optional<std::unique_ptr<szg_image::ImageView>> sceneDepthResult{
            szg_image::ImageView::allocate(
                device,
                allocator,
                szg_image::ImageAllocationParameters{
                    .extent = RESERVED_IMAGE_EXTENT,
                    .format = VK_FORMAT_D32_SFLOAT,
                    .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                | VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                },
                szg_image::ImageViewAllocationParameters{
                    .subresourceRange =
                        vkinit::imageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)
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

void Engine::initWorld(VkDevice const device, VmaAllocator const allocator)
{
    m_camerasBuffer = std::make_unique<TStagedBuffer<gputypes::Camera>>(
        TStagedBuffer<gputypes::Camera>::allocate(
            device,
            allocator,
            CAMERA_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        )
    );
    m_atmospheresBuffer = std::make_unique<TStagedBuffer<gputypes::Atmosphere>>(
        TStagedBuffer<gputypes::Atmosphere>::allocate(
            device,
            allocator,
            ATMOSPHERE_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        )
    );
}

void Engine::initDebug(VkDevice const device, VmaAllocator const allocator)
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
    m_debugLines.vertices =
        std::make_unique<TStagedBuffer<Vertex>>(TStagedBuffer<Vertex>::allocate(
            device,
            allocator,
            DEBUGLINES_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        ));
}

void Engine::initDeferredShadingPipeline(
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

void Engine::initGenericComputePipelines(
    VkDevice const device, scene::SceneTexture const& sceneTexture
)
{
    std::vector<std::string> const shaderPaths{
        "shaders/booleanpush.comp.spv",
        "shaders/gradient_color.comp.spv",
        "shaders/sparse_push_constant.comp.spv",
        "shaders/matrix_color.comp.spv"
    };
    m_genericComputePipeline = std::make_unique<ComputeCollectionPipeline>(
        device, sceneTexture.singletonLayout(), shaderPaths
    );
}

// TODO: Once scenes are made, extract this to a testing scene
#if VKRENDERER_COMPILE_WITH_TESTING
void testDebugLines(float currentTimeSeconds, DebugLines& debugLines)
{
    glm::quat const boxOrientation{glm::toQuat(glm::orientate3(glm::vec3(
        currentTimeSeconds, currentTimeSeconds * glm::euler<float>(), 0.0
    )))};

    debugLines.pushBox(
        glm::vec3(
            3.0 * glm::cos(2.0 * currentTimeSeconds),
            -2.0,
            3.0 * glm::sin(2.0 * currentTimeSeconds)
        ),
        boxOrientation,
        glm::vec3{1.0, 1.0, 1.0}
    );

    debugLines.pushRectangle(
        glm::vec3{2.0, -2.0, 0.0},
        glm::quatLookAt(
            glm::vec3(-1.0, -1.0, 1.0), glm::vec3(-1.0, -1.0, -1.0)
        ),
        glm::vec2{3.0, 1.0}
    );
}
#endif

void Engine::uiEngineControls(ui::DockingLayout const& dockingLayout)
{
    if (ui::UIWindow const engineControls{
            ui::UIWindow::beginDockable("Engine Controls", dockingLayout.right)
        };
        engineControls.open)
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

void Engine::recordDraw(
    VkCommandBuffer const cmd,
    scene::Scene const& scene,
    scene::SceneTexture& sceneTexture,
    std::optional<scene::SceneViewport> const& sceneViewport
)
{
    // Begin scene drawing

    m_debugLines.clear();
    if (!sceneViewport.has_value())
    {
        return;
    }

    std::optional<double> const viewportAspectRatioResult{
        szg_image::aspectRatio(sceneViewport.value().rect.extent)
    };
    if (!viewportAspectRatioResult.has_value())
    {
        return;
    }
    double const aspectRatio{viewportAspectRatioResult.value()};

    { // Copy cameras to gpu
        gputypes::Camera const mainCamera{
            scene.camera.toDeviceEquivalent(static_cast<float>(aspectRatio))
        };

        m_camerasBuffer->clearStaged();
        m_camerasBuffer->push(mainCamera);
        m_camerasBuffer->recordCopyToDevice(cmd);
    }

    std::vector<gputypes::LightDirectional> directionalLights{};
    { // Copy atmospheres to gpu
        scene::AtmosphereBaked const bakedAtmosphere{
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

    for (scene::MeshInstanced const& instance : scene.geometry)
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
                sceneViewport.value().rect,
                sceneTexture.texture().image(),
                *m_sceneDepthTexture,
                directionalLights,
                scene.spotlightsRender ? scene.spotlights
                                       : std::vector<gputypes::LightSpot>{},
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
                cmd,
                cameraIndex,
                sceneTexture,
                sceneViewport.value(),
                *m_camerasBuffer
            );

            break;
        }
        case RenderingPipelines::COMPUTE_COLLECTION:
        {
            m_genericComputePipeline->recordDrawCommands(
                cmd,
                sceneTexture.singletonDescriptor(),
                sceneViewport.value().rect.extent
            );

            break;
        }
        }
    }

    // End scene drawing
}

void Engine::recordDrawDebugLines(
    VkCommandBuffer const cmd,
    uint32_t const cameraIndex,
    scene::SceneTexture& sceneTexture,
    scene::SceneViewport const& sceneViewport,
    TStagedBuffer<gputypes::Camera> const& camerasBuffer
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
                sceneViewport.rect,
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

void Engine::cleanup(VkDevice const device, VmaAllocator const allocator)
{
    if (!m_initialized)
    {
        return;
    }

    SZG_INFO("Engine cleaning up.");

    SZG_CHECK_VK(vkDeviceWaitIdle(device));

    m_genericComputePipeline->cleanup(device);
    m_deferredShadingPipeline->cleanup(device, allocator);

    m_atmospheresBuffer.reset();
    m_camerasBuffer.reset();

    m_debugLines.cleanup(device, allocator);

    m_sceneDepthTexture.reset();

    m_initialized = false;

    SZG_INFO("Engine cleaned up.");
}
