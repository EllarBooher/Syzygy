#include "widgets.hpp"

#include "syzygy/assets/assets.hpp"
#include "syzygy/core/ringbuffer.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/geometry/transform.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/scene.hpp"
#include "syzygy/renderer/scenetexture.hpp"
#include "syzygy/ui/propertytable.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include "syzygy/ui/uiwindow.hpp"
#include <array>
#include <format>
#include <functional>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <implot.h>
#include <memory>
#include <span>
#include <spdlog/fmt/bundled/core.h>
#include <utility>
#include <vector>

void syzygy::performanceWindow(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    RingBuffer const& values,
    float& targetFPS
)
{
    syzygy::UIWindow const window{syzygy::UIWindow::beginDockable(
        std::format("{}##performance", title), dockNode
    )};
    if (!window.isOpen())
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
    syzygy::Atmosphere& atmosphere, syzygy::Atmosphere const& defaultValues
)
{
    float constexpr EULER_ANGLES_SPEED{0.1F};

    syzygy::FloatBounds constexpr RGBA_BOUNDS{0.0F, 1.0F};

    float constexpr PLANETARY_RADIUS_MIN{1.0F};
    float constexpr PLANETARY_RADIUS_MAX{1'000'000'000.0F};

    // Scattering coefficient meaningfully exists over a very small and
    // unpredictable range. Thus finer controls are needed, and a speed of 0.1
    // or default 0.0 is too high.
    float constexpr SCATTERING_COEFFICIENT_SPEED{0.01F};
    syzygy::FloatBounds constexpr SCATTERING_COEFFICIENT_BOUNDS{0.0F, 1.0F};

    syzygy::FloatBounds constexpr ALTITUDE_DECAY_BOUNDS{0.0F, 1'000'000.0F};

    syzygy::PropertyTable::begin()
        .rowVec3(
            "Sun Euler Angles",
            atmosphere.sunEulerAngles,
            defaultValues.sunEulerAngles,
            syzygy::PropertySliderBehavior{
                .speed = EULER_ANGLES_SPEED,
            }
        )
        .rowReadOnlyVec3("Direction to Sun", atmosphere.directionToSun())
        .rowVec3(
            "Ground Diffuse Color",
            atmosphere.groundColor,
            defaultValues.groundColor,
            syzygy::PropertySliderBehavior{
                .bounds = RGBA_BOUNDS,
            }
        )
        .rowFloat(
            "Earth Radius",
            atmosphere.earthRadiusMeters,
            defaultValues.earthRadiusMeters,
            syzygy::PropertySliderBehavior{
                .bounds =
                    syzygy::FloatBounds{
                        PLANETARY_RADIUS_MIN, atmosphere.atmosphereRadiusMeters
                    },
            }
        )
        .rowFloat(
            "Atmosphere Radius",
            atmosphere.atmosphereRadiusMeters,
            defaultValues.atmosphereRadiusMeters,
            syzygy::PropertySliderBehavior{
                .bounds =
                    syzygy::FloatBounds{
                        atmosphere.earthRadiusMeters, PLANETARY_RADIUS_MAX
                    },
            }
        )
        .rowVec3(
            "Rayleigh Scattering Coefficient",
            atmosphere.scatteringCoefficientRayleigh,
            defaultValues.scatteringCoefficientRayleigh,
            syzygy::PropertySliderBehavior{
                .speed = SCATTERING_COEFFICIENT_SPEED,
                .bounds = SCATTERING_COEFFICIENT_BOUNDS,
            }
        )
        .rowFloat(
            "Rayleigh Altitude Decay",
            atmosphere.altitudeDecayRayleigh,
            defaultValues.altitudeDecayRayleigh,
            syzygy::PropertySliderBehavior{
                .bounds = ALTITUDE_DECAY_BOUNDS,
            }
        )
        .rowVec3(
            "Mie Scattering Coefficient",
            atmosphere.scatteringCoefficientMie,
            defaultValues.scatteringCoefficientMie,
            syzygy::PropertySliderBehavior{
                .speed = SCATTERING_COEFFICIENT_SPEED,
                .bounds = SCATTERING_COEFFICIENT_BOUNDS,
            }
        )
        .rowFloat(
            "Mie Altitude Decay",
            atmosphere.altitudeDecayMie,
            defaultValues.altitudeDecayMie,
            syzygy::PropertySliderBehavior{
                .bounds = ALTITUDE_DECAY_BOUNDS,
            }
        )
        .end();
}
void uiCamera(
    syzygy::Camera& camera,
    syzygy::Camera const& defaultValues,
    float& cameraSpeed,
    float const& defaultCameraSpeed
)
{
    // Stay an arbitrary distance away 0 and 180 degrees to avoid singularities
    syzygy::FloatBounds constexpr FOV_BOUNDS{0.01F, 179.99F};

    float constexpr CLIPPING_PLANE_MIN{0.01F};
    float constexpr CLIPPING_PlANE_MAX{1'000'000.0F};

    float constexpr CLIPPING_PLANE_MARGIN{0.01F};

    syzygy::PropertyTable::begin()
        .rowFloat(
            "Editor Movement Speed",
            cameraSpeed,
            defaultCameraSpeed,
            syzygy::PropertySliderBehavior{
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
            syzygy::PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .rowVec3(
            "Euler Angles",
            camera.eulerAngles,
            defaultValues.eulerAngles,
            syzygy::PropertySliderBehavior{
                .bounds =
                    syzygy::FloatBounds{-glm::pi<float>(), glm::pi<float>()},
            }
        )
        .rowFloat(
            "Field of View",
            camera.fovDegrees,
            defaultValues.fovDegrees,
            syzygy::PropertySliderBehavior{
                .bounds = FOV_BOUNDS,
            }
        )
        .rowFloat(
            "Near Plane",
            camera.near,
            std::min(camera.far, defaultValues.near),
            syzygy::PropertySliderBehavior{
                .bounds = syzygy::FloatBounds{CLIPPING_PLANE_MIN, camera.far},
            }
        )
        .rowFloat(
            "Far Plane",
            camera.far,
            std::max(camera.near, defaultValues.far),
            syzygy::PropertySliderBehavior{
                .bounds =
                    syzygy::FloatBounds{
                        camera.near + CLIPPING_PLANE_MARGIN, CLIPPING_PlANE_MAX
                    },
            }
        )
        .end();
}
void uiTransform(syzygy::PropertyTable& table, syzygy::Transform& transform)
{
    table.rowVec3(
        "Translation",
        transform.translation,
        glm::vec3{0.0F},
        syzygy::PropertySliderBehavior{.speed = 1.0F}
    );
    table.rowVec3(
        "Euler Angles (Radians)",
        transform.eulerAnglesRadians,
        glm::vec3{0.0F},
        syzygy::PropertySliderBehavior{
            .bounds = syzygy::FloatBounds{-glm::pi<float>(), glm::pi<float>()},
        }
    );
    table.rowVec3(
        "Scale",
        transform.scale,
        glm::vec3{1.0F},
        syzygy::PropertySliderBehavior{
            .bounds = syzygy::FloatBounds{.min = 0.0F, .max = 100.0F}
        }
    );
}
void uiInstanceAnimation(syzygy::InstanceAnimation& animation)
{

    // Assumes no gaps in enum values
    std::array<
        char const*,
        static_cast<size_t>(syzygy::InstanceAnimation::LAST) + 1>
        labels{"Unknown"};
    labels[static_cast<size_t>(syzygy::InstanceAnimation::None)] = "None";
    labels[static_cast<size_t>(syzygy::InstanceAnimation::Diagonal_Wave)] =
        "Diagonal Wave";
    labels[static_cast<size_t>(syzygy::InstanceAnimation::Spin_Along_World_Up
    )] = "Spin Along World Up";

    auto const selectedAnimationIndex{static_cast<size_t>(animation)};
    std::string const previewLabel{
        selectedAnimationIndex >= labels.size() ? "Unknown"
                                                : labels[selectedAnimationIndex]
    };

    if (ImGui::BeginCombo(
            "##instanceAnimation", labels[selectedAnimationIndex]
        ))
    {
        size_t constexpr FIRST_INDEX{
            static_cast<size_t>(syzygy::InstanceAnimation::FIRST)
        };
        size_t constexpr LAST_INDEX{
            static_cast<size_t>(syzygy::InstanceAnimation::LAST)
        };
        for (size_t index{FIRST_INDEX}; index <= LAST_INDEX; index++)
        {
            if (ImGui::Selectable(
                    labels[index], selectedAnimationIndex == index
                ))
            {
                animation = static_cast<syzygy::InstanceAnimation>(index);
                break;
            }
        }
        ImGui::EndCombo();
    }
}
auto uiMeshSelection(
    std::optional<std::reference_wrapper<syzygy::MeshAsset>> const currentMesh,
    std::span<syzygy::AssetRef<syzygy::MeshAsset> const> const meshes
) -> std::optional<std::shared_ptr<syzygy::MeshAsset>>
{
    ImGui::BeginDisabled(meshes.empty());

    std::optional<std::shared_ptr<syzygy::MeshAsset>> newMesh{std::nullopt};

    std::string const previewLabel{
        currentMesh.has_value() ? currentMesh.value().get().name : "None"
    };
    if (ImGui::BeginCombo("##meshSelection", previewLabel.c_str()))
    {
        size_t const index{0};
        for (auto const& assetRef : meshes)
        {
            syzygy::Asset<syzygy::MeshAsset> const& asset{assetRef.get()};

            if (asset.data == nullptr)
            {
                continue;
            }

            syzygy::MeshAsset const& mesh{*asset.data};
            bool const selected{
                currentMesh.has_value()
                && asset.data.get() == &currentMesh.value().get()
            };

            if (ImGui::Selectable(mesh.name.c_str(), selected))
            {
                newMesh = asset.data;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::EndDisabled();

    return newMesh;
}

void uiSceneGeometry(
    syzygy::AABB& bounds,
    std::span<syzygy::MeshInstanced> const geometry,
    std::span<syzygy::AssetRef<syzygy::MeshAsset> const> const meshes
)
{
    syzygy::PropertyTable table{syzygy::PropertyTable::begin()};

    table.rowChildPropertyBegin("Scene Bounds")
        .rowVec3(
            "Scene Center",
            bounds.center,
            bounds.center,
            syzygy::PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .rowVec3(
            "Scene Half-Extent",
            bounds.halfExtent,
            bounds.halfExtent,
            syzygy::PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .childPropertyEnd();

    for (syzygy::MeshInstanced& instance : geometry)
    {
        table.rowChildPropertyBegin(instance.name);
        table.rowBoolean("Render", instance.render, true);
        for (syzygy::Transform& transform : instance.originals)
        {
            uiTransform(table, transform);
        }
        table.rowCustom(
            "Instance Animation",
            [&]() { uiInstanceAnimation(instance.animation); }
        );
        table.rowCustom(
            "Mesh Used",
            [&]()
        {
            auto newMesh{uiMeshSelection(instance.getMesh(), meshes)};
            if (newMesh.has_value())
            {
                instance.setMesh(newMesh.value());
            }
        }
        );
        table.childPropertyEnd();
    }

    table.end();
}
} // namespace

namespace syzygy
{
void sceneControlsWindow(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    Scene& scene,
    std::span<AssetRef<MeshAsset> const> const meshes
)
{
    UIWindow const window{
        UIWindow::beginDockable(std::format("{}##scene", title), dockNode)
    };
    if (!window.isOpen())
    {
        return;
    }

    if (ImGui::CollapsingHeader(
            "Sun Animation", ImGuiTreeNodeFlags_DefaultOpen
        ))
    {
        SunAnimation const& defaultAnimation{Scene::DEFAULT_SUN_ANIMATION};

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
        uiAtmosphere(scene.atmosphere, Scene::DEFAULT_ATMOSPHERE_EARTH);
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        uiCamera(
            scene.camera,
            Scene::DEFAULT_CAMERA,
            scene.cameraControlledSpeed,
            Scene::DEFAULT_CAMERA_CONTROLLED_SPEED
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

auto sceneViewportWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    std::optional<UIRectangle> maximizeArea,
    SceneTexture const& texture,
    bool const focused
) -> WindowResult<std::optional<VkRect2D>>
{
    uint16_t pushedStyleColors{0};
    if (focused)
    {
        ImVec4 const activeTitleColor{
            ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive)
        };
        ImGui::PushStyleColor(ImGuiCol_WindowBg, activeTitleColor);
        pushedStyleColors += 1;
    }

    UIWindow sceneViewport{
        maximizeArea.has_value()
            ? UIWindow::beginMaximized(title, maximizeArea.value())
            : UIWindow::beginDockable(title, dockNode)
    };

    if (!sceneViewport.isOpen())
    {
        return {
            .focused = false,
            .payload = std::nullopt,
        };
    }

    glm::vec2 const contentExtent{sceneViewport.screenRectangle().size()};

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

    VkRect2D const renderedSubregion{
        .offset = {0, 0},
        .extent =
            VkExtent2D{
                .width = static_cast<uint32_t>(contentExtent.x),
                .height = static_cast<uint32_t>(contentExtent.y),
            }
    };

    return {
        .focused = clicked,
        .payload = renderedSubregion,
    };
}
} // namespace syzygy