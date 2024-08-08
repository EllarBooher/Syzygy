#include "widgets.hpp"

#include "syzygy/assets.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/core/scene.hpp"
#include "syzygy/enginetypes.hpp"
#include "syzygy/images/image.hpp"
#include "syzygy/images/imageview.hpp"
#include "syzygy/ui/propertytable.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include "syzygy/ui/uiwindow.hpp"
#include "syzygy/vulkanusage.hpp"
#include <fmt/core.h>
#include <format>
#include <functional>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <implot.h>
#include <memory>
#include <span>
#include <utility>
#include <vector>

void ui::performanceWindow(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    RingBuffer const& values,
    float& targetFPS
)
{
    ui::UIWindow const window{ui::UIWindow::beginDockable(
        std::format("{}##performance", title), dockNode
    )};
    if (!window.open)
    {
        return;
    }

    ImGui::Text("%s", fmt::format("FPS: {:.1f}", values.average()).c_str());
    float const minFPS{10.0};
    float const maxFPS{1000.0};
    ImGui::DragScalar(
        "Target FPS",
        ImGuiDataType_Float,
        &targetFPS,
        1.0,
        &minFPS,
        &maxFPS,
        nullptr,
        ImGuiSliderFlags_AlwaysClamp
    );

    ImVec2 const plotSize{-1, 200};

    if (ImPlot::BeginPlot("FPS", plotSize))
    {
        ImPlot::SetupAxes(
            "",
            "FPS",
            ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock,
            ImPlotAxisFlags_LockMin
        );

        double constexpr DISPLAYED_FPS_MIN{0.0};
        double constexpr DISPLAYED_FPS_MAX{320.0};

        std::span<double const> const fpsValues{values.values()};

        ImPlot::SetupAxesLimits(
            0,
            static_cast<double>(fpsValues.size()),
            DISPLAYED_FPS_MIN,
            DISPLAYED_FPS_MAX
        );

        ImPlot::PlotLine(
            "##fpsValues",
            fpsValues.data(),
            static_cast<int32_t>(fpsValues.size())
        );

        size_t const currentIndex{values.current()};
        ImPlot::PlotInfLines("##current", &currentIndex, 1);

        ImPlot::EndPlot();
    }
}

namespace
{
void uiAtmosphere(
    scene::Atmosphere& atmosphere, scene::Atmosphere const& defaultValues
)
{
    float constexpr EULER_ANGLES_SPEED{0.1F};

    FloatBounds constexpr RGBA_BOUNDS{0.0F, 1.0F};

    float constexpr PLANETARY_RADIUS_MIN{1.0F};
    float constexpr PLANETARY_RADIUS_MAX{1'000'000'000.0F};

    // Scattering coefficient meaningfully exists over a very small and
    // unpredictable range. Thus finer controls are needed, and a speed of 0.1
    // or default 0.0 is too high.
    float constexpr SCATTERING_COEFFICIENT_SPEED{0.01F};
    FloatBounds constexpr SCATTERING_COEFFICIENT_BOUNDS{0.0F, 1.0F};

    FloatBounds constexpr ALTITUDE_DECAY_BOUNDS{0.0F, 1'000'000.0F};

    PropertyTable::begin()
        .rowVec3(
            "Sun Euler Angles",
            atmosphere.sunEulerAngles,
            defaultValues.sunEulerAngles,
            PropertySliderBehavior{
                .speed = EULER_ANGLES_SPEED,
            }
        )
        .rowReadOnlyVec3("Direction to Sun", atmosphere.directionToSun())
        .rowVec3(
            "Ground Diffuse Color",
            atmosphere.groundColor,
            defaultValues.groundColor,
            PropertySliderBehavior{
                .bounds = RGBA_BOUNDS,
            }
        )
        .rowFloat(
            "Earth Radius",
            atmosphere.earthRadiusMeters,
            defaultValues.earthRadiusMeters,
            PropertySliderBehavior{
                .bounds =
                    FloatBounds{
                        PLANETARY_RADIUS_MIN, atmosphere.atmosphereRadiusMeters
                    },
            }
        )
        .rowFloat(
            "Atmosphere Radius",
            atmosphere.atmosphereRadiusMeters,
            defaultValues.atmosphereRadiusMeters,
            PropertySliderBehavior{
                .bounds =
                    FloatBounds{
                        atmosphere.earthRadiusMeters, PLANETARY_RADIUS_MAX
                    },
            }
        )
        .rowVec3(
            "Rayleigh Scattering Coefficient",
            atmosphere.scatteringCoefficientRayleigh,
            defaultValues.scatteringCoefficientRayleigh,
            PropertySliderBehavior{
                .speed = SCATTERING_COEFFICIENT_SPEED,
                .bounds = SCATTERING_COEFFICIENT_BOUNDS,
            }
        )
        .rowFloat(
            "Rayleigh Altitude Decay",
            atmosphere.altitudeDecayRayleigh,
            defaultValues.altitudeDecayRayleigh,
            PropertySliderBehavior{
                .bounds = ALTITUDE_DECAY_BOUNDS,
            }
        )
        .rowVec3(
            "Mie Scattering Coefficient",
            atmosphere.scatteringCoefficientMie,
            defaultValues.scatteringCoefficientMie,
            PropertySliderBehavior{
                .speed = SCATTERING_COEFFICIENT_SPEED,
                .bounds = SCATTERING_COEFFICIENT_BOUNDS,
            }
        )
        .rowFloat(
            "Mie Altitude Decay",
            atmosphere.altitudeDecayMie,
            defaultValues.altitudeDecayMie,
            PropertySliderBehavior{
                .bounds = ALTITUDE_DECAY_BOUNDS,
            }
        )
        .end();
}
void uiCamera(
    scene::Camera& camera,
    scene::Camera const& defaultValues,
    float& cameraSpeed,
    float const& defaultCameraSpeed
)
{
    // Stay an arbitrary distance away 0 and 180 degrees to avoid singularities
    FloatBounds constexpr FOV_BOUNDS{0.01F, 179.99F};

    float constexpr CLIPPING_PLANE_MIN{0.01F};
    float constexpr CLIPPING_PlANE_MAX{1'000'000.0F};

    float constexpr CLIPPING_PLANE_MARGIN{0.01F};

    PropertyTable::begin()
        .rowFloat(
            "Editor Movement Speed",
            cameraSpeed,
            defaultCameraSpeed,
            PropertySliderBehavior{
                .bounds = {0.0F, 100.0F},
            }
        )
        .rowBoolean(
            "Orthographic", camera.orthographic, defaultValues.orthographic
        )
        .rowVec3(
            "Camera Position",
            camera.cameraPosition,
            defaultValues.cameraPosition,
            PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .rowVec3(
            "Euler Angles",
            camera.eulerAngles,
            defaultValues.eulerAngles,
            PropertySliderBehavior{
                .bounds = FloatBounds{-glm::pi<float>(), glm::pi<float>()},
            }
        )
        .rowFloat(
            "Field of View",
            camera.fovDegrees,
            defaultValues.fovDegrees,
            PropertySliderBehavior{
                .bounds = FOV_BOUNDS,
            }
        )
        .rowFloat(
            "Near Plane",
            camera.near,
            std::min(camera.far, defaultValues.near),
            PropertySliderBehavior{
                .bounds = FloatBounds{CLIPPING_PLANE_MIN, camera.far},
            }
        )
        .rowFloat(
            "Far Plane",
            camera.far,
            std::max(camera.near, defaultValues.far),
            PropertySliderBehavior{
                .bounds =
                    FloatBounds{
                        camera.near + CLIPPING_PLANE_MARGIN, CLIPPING_PlANE_MAX
                    },
            }
        )
        .end();
}
void uiSceneGeometry(
    scene::SceneBounds& bounds,
    std::span<scene::MeshInstanced> const geometry,
    MeshAssetLibrary const& meshes
)
{
    PropertyTable table{PropertyTable::begin()};

    table.rowChildPropertyBegin("Scene Bounds")
        .rowVec3(
            "Scene Center",
            bounds.center,
            bounds.center,
            PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .rowVec3(
            "Scene Extent",
            bounds.extent,
            bounds.extent,
            PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .childPropertyEnd();

    for (scene::MeshInstanced& instance : geometry)
    {
        table.rowChildPropertyBegin(instance.name);
        table.rowBoolean("Render", instance.render, true);
        table.rowCustom(
            "Mesh Used",
            [&]()
        {
            ImGui::BeginDisabled(meshes.loadedMeshes.empty());

            std::string const previewLabel{
                instance.mesh == nullptr ? "None" : instance.mesh->name
            };
            if (ImGui::BeginCombo("##meshSelection", previewLabel.c_str()))
            {
                size_t const index{0};
                for (std::shared_ptr<MeshAsset> const& pMesh :
                     meshes.loadedMeshes)
                {
                    if (pMesh == nullptr)
                    {
                        continue;
                    }

                    MeshAsset const& mesh{*pMesh};
                    bool const selected{pMesh == instance.mesh};

                    if (ImGui::Selectable(mesh.name.c_str(), selected))
                    {
                        instance.mesh = pMesh;
                        break;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::EndDisabled();
        }
        );
        table.childPropertyEnd();
    }

    table.end();
}
} // namespace

void ui::sceneControlsWindow(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    scene::Scene& scene,
    MeshAssetLibrary const& meshes
)
{
    ui::UIWindow const window{
        ui::UIWindow::beginDockable(std::format("{}##scene", title), dockNode)
    };
    if (!window.open)
    {
        return;
    }

    if (ImGui::CollapsingHeader(
            "Sun Animation", ImGuiTreeNodeFlags_DefaultOpen
        ))
    {
        scene::SunAnimation const& defaultAnimation{
            scene::Scene::DEFAULT_SUN_ANIMATION
        };

        FloatBounds constexpr SUN_ANIMATION_SPEED_BOUNDS{
            -100'000.0F, 100'000.0F
        };

        PropertyTable::begin()
            .rowBoolean(
                "Frozen", scene.sunAnimation.frozen, defaultAnimation.frozen
            )
            .rowFloat(
                "Time",
                scene.sunAnimation.time,
                defaultAnimation.time,
                PropertySliderBehavior{
                    .bounds = {0.0F, 1.0F},
                }
            )
            .rowFloat(
                "Speed",
                scene.sunAnimation.speed,
                defaultAnimation.speed,
                PropertySliderBehavior{
                    .bounds = SUN_ANIMATION_SPEED_BOUNDS,
                }
            )
            .rowBoolean(
                "Skip Night",
                scene.sunAnimation.skipNight,
                defaultAnimation.skipNight
            )
            .end();
    }

    if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
    {
        uiAtmosphere(scene.atmosphere, scene::Scene::DEFAULT_ATMOSPHERE_EARTH);
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        uiCamera(
            scene.camera,
            scene::Scene::DEFAULT_CAMERA,
            scene.cameraControlledSpeed,
            scene::Scene::DEFAULT_CAMERA_CONTROLLED_SPEED
        );
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
    {
        PropertyTable::begin()
            .rowBoolean("Render Spotlights", scene.spotlightsRender, true)
            .end();
    }

    if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen))
    {
        uiSceneGeometry(scene.bounds, scene.geometry, meshes);
    }
}

auto ui::sceneViewportWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    std::optional<UIRectangle> maximizeArea,
    scene::SceneTexture const& texture,
    bool const focused
) -> WindowResult<std::optional<scene::SceneViewport>>
{
    size_t pushedStyleColors{0};
    if (focused)
    {
        ImVec4 const activeTitleColor{
            ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive)
        };
        ImGui::PushStyleColor(ImGuiCol_WindowBg, activeTitleColor);
        pushedStyleColors += 1;
    }

    ui::UIWindow sceneViewport{
        maximizeArea.has_value()
            ? ui::UIWindow::beginMaximized(title, maximizeArea.value())
            : ui::UIWindow::beginDockable(title, dockNode)
    };

    if (!sceneViewport.open)
    {
        return {
            .focused = false,
            .payload = std::nullopt,
        };
    }

    glm::vec2 const contentExtent{sceneViewport.screenRectangle.size()};

    VkExtent2D const textureMax{texture.texture().image().extent2D()};

    ImVec2 const uvMax{
        contentExtent.x / static_cast<float>(textureMax.width),
        contentExtent.y / static_cast<float>(textureMax.height)
    };

    float const textHeight{
        ImGui::CalcTextSize("").y + ImGui::GetStyle().ItemSpacing.y
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{});
    bool const clicked = ImGui::ImageButton(
        "##viewport",
        reinterpret_cast<ImTextureID>(texture.imguiDescriptor()),
        contentExtent - glm::vec2{0.0F, textHeight},
        ImVec2{0.0, 0.0},
        uvMax,
        ImVec4{0.0F, 0.0F, 0.0F, 0.0F},
        ImVec4{1.0F, 1.0F, 1.0F, 1.0F}
    );
    ImGui::PopStyleVar();

    ImGui::Text("Click Scene Viewport to capture inputs. Translate Camera: "
                "WASD + QE. Rotate Camera: Mouse. Stop Capturing: TAB.");
    sceneViewport.end();

    ImGui::PopStyleColor(pushedStyleColors);

    return {
        .focused = clicked,
        .payload =
            scene::SceneViewport{
                .rect =
                    VkRect2D{
                        .offset = {0, 0},
                        .extent =
                            VkExtent2D{
                                .width = static_cast<uint32_t>(contentExtent.x),
                                .height =
                                    static_cast<uint32_t>(contentExtent.y),
                            }
                    }
            }
    };
}
