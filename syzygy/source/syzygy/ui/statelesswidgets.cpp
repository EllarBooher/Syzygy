#include "statelesswidgets.hpp"

#include "syzygy/assets/assets.hpp"
#include "syzygy/assets/assetstypes.hpp"
#include "syzygy/core/ringbuffer.hpp"
#include "syzygy/core/uuid.hpp"
#include "syzygy/editor/editorconfig.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/geometry/transform.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/material.hpp"
#include "syzygy/renderer/scene.hpp"
#include "syzygy/ui/propertytable.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include "syzygy/ui/uiwindowscope.hpp"
#include <array>
#include <format>
#include <functional>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <implot.h>
#include <memory>
#include <span>
#include <spdlog/fmt/bundled/core.h>
#include <utility>
#include <vector>

namespace
{
auto constexpr to_string(syzygy::GammaTransferFunction const transferFunction)
    -> char const*
{
    switch (transferFunction)
    {
    case syzygy::GammaTransferFunction::PureGamma:
        return "Pure Gamma 2.2";
    case syzygy::GammaTransferFunction::sRGB:
        return "sRGB (piecewise)";
    case syzygy::GammaTransferFunction::MAX:
        return "Invalid Transfer Function";
    default:
        return "Unknown Transfer Function";
    }
}
} // namespace

namespace syzygy
{
void editorConfigurationWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    EditorConfiguration& value,
    EditorConfiguration const& /*defaults*/
)
{
    UIWindowScope const window{UIWindowScope::beginDockable(
        fmt::format("{}##editorConfiguration", title), dockNode
    )};
    if (!window.isOpen())
    {
        return;
    }

    syzygy::PropertyTable::begin()
        .rowCustom(
            "Gamma Transfer Function",
            [&]()
    {
        GammaTransferFunction& selectedFunction{value.transferFunction};
        if (ImGui::BeginCombo(
                "##gammeTransferFunction", to_string(selectedFunction)
            ))
        {
            for (size_t functionIndex{0};
                 functionIndex
                 < static_cast<size_t>(GammaTransferFunction::MAX);
                 functionIndex++)
            {
                auto const function{
                    static_cast<GammaTransferFunction>(functionIndex)
                };
                if (ImGui::Selectable(
                        to_string(function), selectedFunction == function
                    ))
                {
                    selectedFunction = function;
                }
            }
            ImGui::EndCombo();
        }
    }
        )
        .end();
}
} // namespace syzygy

void syzygy::performanceWindow(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    RingBuffer const& values,
    float& targetFPS
)
{
    syzygy::UIWindowScope const window{syzygy::UIWindowScope::beginDockable(
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
    float constexpr SCATTERING_COEFFICIENT_SPEED{0.00001F};
    float constexpr KILOMETERS_PER_MEGAMETER{1'000.0F};
    float constexpr RADIANS_PER_ARCMINUTE{
        (1.0F / 60.0F) * (glm::pi<float>() / 180.0F)
    };
    syzygy::FloatBounds constexpr SCATTERING_COEFFICIENT_BOUNDS{0.0F, 1.0F};

    syzygy::FloatBounds constexpr ALTITUDE_DECAY_BOUNDS{0.0F, 1'000'000.0F};

    syzygy::PropertySliderBehavior constexpr EXTINCTION_COEFFICIENT_BEHAVIOR{
        .speed = 1.0F,
        .bounds = syzygy::FloatBounds{.min = 0.0F},
    };
    syzygy::PropertySliderBehavior constexpr ALTITUDE_DECAY_BEHAVIOR{
        .speed = 0.01F,
        .bounds = syzygy::FloatBounds{.min = 0.0F},
    };

    syzygy::PropertyTable::begin()
        .rowColor(
            "Ground Diffuse Color",
            atmosphere.groundColor,
            defaultValues.groundColor,
            syzygy::PropertySliderBehavior{.bounds = RGBA_BOUNDS}
        )
        .rowFloat(
            "Earth Radius (Mm)",
            atmosphere.planetRadiusMegameters,
            defaultValues.planetRadiusMegameters,
            syzygy::PropertySliderBehavior{
                .speed = 1.0F / KILOMETERS_PER_MEGAMETER,
                .bounds =
                    syzygy::FloatBounds{
                        PLANETARY_RADIUS_MIN,
                        atmosphere.atmosphereRadiusMegameters
                    },
            }
        )
        .rowFloat(
            "Atmosphere Radius (Mm)",
            atmosphere.atmosphereRadiusMegameters,
            defaultValues.atmosphereRadiusMegameters,
            syzygy::PropertySliderBehavior{
                .speed = 1.0F / KILOMETERS_PER_MEGAMETER,
                .bounds =
                    syzygy::FloatBounds{
                        atmosphere.planetRadiusMegameters, PLANETARY_RADIUS_MAX
                    },
            }
        )
        .rowColor(
            "Rayleigh Scattering (per Mm)",
            atmosphere.scatteringRayleighPerMegameter,
            defaultValues.scatteringRayleighPerMegameter,
            EXTINCTION_COEFFICIENT_BEHAVIOR
        )
        .rowColor(
            "Rayleigh Absorption (per Mm)",
            atmosphere.absorptionRayleighPerMegameter,
            defaultValues.absorptionRayleighPerMegameter,
            EXTINCTION_COEFFICIENT_BEHAVIOR
        )
        .rowFloat(
            "Rayleigh Altitude Decay (Mm)",
            atmosphere.altitudeDecayRayleighMegameters,
            defaultValues.altitudeDecayRayleighMegameters,
            ALTITUDE_DECAY_BEHAVIOR
        )
        .rowColor(
            "Mie Scattering (per Mm)",
            atmosphere.scatteringMiePerMegameter,
            defaultValues.scatteringMiePerMegameter,
            EXTINCTION_COEFFICIENT_BEHAVIOR
        )
        .rowColor(
            "Mie Absorption (per Mm)",
            atmosphere.absorptionMiePerMegameter,
            defaultValues.absorptionMiePerMegameter,
            EXTINCTION_COEFFICIENT_BEHAVIOR
        )
        .rowFloat(
            "Mie Altitude Decay (Mm)",
            atmosphere.altitudeDecayMieMegameters,
            defaultValues.altitudeDecayMieMegameters,
            ALTITUDE_DECAY_BEHAVIOR
        )
        .rowColor(
            "Ozone Scattering (per Mm)",
            atmosphere.scatteringOzonePerMegameter,
            defaultValues.scatteringOzonePerMegameter,
            EXTINCTION_COEFFICIENT_BEHAVIOR
        )
        .rowColor(
            "Ozone Absorption (per Mm)",
            atmosphere.absorptionOzonePerMegameter,
            defaultValues.absorptionOzonePerMegameter,
            EXTINCTION_COEFFICIENT_BEHAVIOR
        )
        .end();
}
void uiAtmosphereLights(std::span<syzygy::DirectionalLight> const lights)
{
    auto table{syzygy::PropertyTable::begin()};

    syzygy::PropertySliderBehavior constexpr RGB_BEHAVIOR{
        .speed = 0.0F,
        .bounds = syzygy::FloatBounds{.min = 0.0F, .max = 1.0F},
    };
    syzygy::PropertySliderBehavior constexpr STRENGTH_BEHAVIOR{
        .speed = 0.01F,
        .bounds = syzygy::FloatBounds{.min = 0.0F},
    };
    syzygy::PropertySliderBehavior constexpr EULER_ANGLES_BEHAVIOR{
        .speed = 0.1F,
    };

    float constexpr RADIANS_PER_ARCMINUTE{
        (1.0F / 60.0F) * (glm::pi<float>() / 180.0F)
    };
    float constexpr DEFAULT_ANGULAR_RADIUS{RADIANS_PER_ARCMINUTE * 16.0F};

    syzygy::PropertySliderBehavior constexpr AZIMUTH_BEHAVIOR{
        .speed = 0.0F,
        .bounds = syzygy::FloatBounds{.min = 0.0F, .max = glm::two_pi<float>()}
    };

    syzygy::PropertySliderBehavior constexpr ORBITAL_PERIOD_BEHAVIOR{
        .speed = 0.1F,
        .bounds = syzygy::FloatBounds{.min = 0.0F},
    };

    syzygy::PropertySliderBehavior constexpr ANGULAR_RADIUS_BEHAVIOR{
        .speed = RADIANS_PER_ARCMINUTE,
        .bounds = syzygy::FloatBounds{.min = 0.0F}
    };

    for (auto& light : lights)
    {
        table.rowChildPropertyBegin(light.name, false)
            .rowColor("Color", light.color, glm::vec3{1.0F}, RGB_BEHAVIOR)
            .rowFloat("Strength", light.strength, 1.0F, STRENGTH_BEHAVIOR)
            .rowReadOnlyFloat("Zenith", light.zenith)
            .rowFloat("Azimuth", light.azimuth, 0.0F, AZIMUTH_BEHAVIOR)
            .rowFloat(
                "Orbital Period (Days)",
                light.orbitalPeriodDays,
                1.0F,
                ORBITAL_PERIOD_BEHAVIOR
            )
            .rowFloat(
                "Angular Radius",
                light.angularRadius,
                DEFAULT_ANGULAR_RADIUS,
                ANGULAR_RADIUS_BEHAVIOR
            )
            .rowReadOnlyVec3("Incident Direction", light.forward())
            .childPropertyEnd();
    }
    table.end();
}
void uiCamera(
    syzygy::Camera& camera,
    syzygy::Camera const& defaultValues,
    float& cameraSpeed,
    float const& defaultCameraSpeed
)
{
    // Stay an arbitrary distance away 0 and 180 degrees to avoid
    // singularities
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
void uiTransform(
    syzygy::PropertyTable& table,
    syzygy::Transform& transform,
    syzygy::Transform const& original
)
{
    table.rowVec3(
        "Translation",
        transform.translation,
        original.translation,
        syzygy::PropertySliderBehavior{.speed = 1.0F}
    );
    table.rowVec3(
        "Euler Angles (Radians)",
        transform.eulerAnglesRadians,
        original.eulerAnglesRadians,
        syzygy::PropertySliderBehavior{
            .bounds = syzygy::FloatBounds{-glm::pi<float>(), glm::pi<float>()},
        }
    );
    table.rowVec3(
        "Scale",
        transform.scale,
        original.scale,
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
template <typename T>
auto uiAssetSelection(
    std::optional<syzygy::AssetRef<T>> const& currentAsset,
    std::span<syzygy::AssetPtr<T> const> const possibleAssets
) -> std::optional<syzygy::AssetPtr<T>>
{
    ImGui::BeginDisabled(possibleAssets.empty());

    std::optional<syzygy::AssetPtr<T>> newAsset{std::nullopt};

    bool const currentAssetIsValid{currentAsset.has_value()};
    std::string const previewLabel{
        currentAssetIsValid ? currentAsset.value().get().metadata.displayName
                            : "None"
    };
    if (ImGui::BeginCombo("##meshSelection", previewLabel.c_str()))
    {
        size_t const index{0};
        {
            bool const noneSelected{!currentAssetIsValid};

            if (ImGui::Selectable("None", !currentAssetIsValid))
            {
                newAsset = syzygy::AssetPtr<T>{};
            }
        }
        for (auto const& pAsset : possibleAssets)
        {
            if (pAsset.lock() == nullptr)
            {
                continue;
            }
            syzygy::Asset<T> const& asset{*pAsset.lock()};

            bool const selected{
                currentAssetIsValid
                && asset.metadata.id == currentAsset.value().get().metadata.id
            };

            if (ImGui::Selectable(
                    fmt::format("{}##{}", asset.metadata.displayName, index)
                        .c_str(),
                    selected
                ))
            {
                newAsset = pAsset;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::EndDisabled();

    return newAsset;
}

void uiAssetMetadata(
    syzygy::PropertyTable& table, syzygy::AssetMetadata const& metadata
)
{
    uint64_t const id{static_cast<uint64_t>(metadata.id)};

    table.rowChildPropertyBegin("Asset Metadata");
    table.rowReadOnlyTextInput("Display Name", metadata.displayName, false);
    table.rowReadOnlyTextInput(
        "Global Identifier", fmt::format("{:x}", id), false
    );
    table.rowReadOnlyTextInput("Path on Disk", metadata.fileLocalPath, false);
    table.childPropertyEnd();
}

template <typename T>
void uiAssetReadOnlyNameField(
    syzygy::PropertyTable& table,
    std::string const& rowName,
    syzygy::AssetPtr<T> const& asset
)
{
    table.rowReadOnlyTextInput(
        rowName,
        asset.lock() == nullptr ? "None" : asset.lock()->metadata.displayName,
        false
    );
}

void uiMesh(syzygy::PropertyTable& table, syzygy::Mesh const& mesh)
{
    table.rowChildPropertyBegin("Mesh AABB");
    table.rowReadOnlyVec3("Center", mesh.vertexBounds.center);
    table.rowReadOnlyVec3("Half-Extent", mesh.vertexBounds.halfExtent);
    table.childPropertyEnd();

    table.rowChildPropertyBegin("Mesh Surfaces");

    size_t surfaceIndex{0};
    for (syzygy::GeometrySurface const& surface : mesh.surfaces)
    {
        table.rowChildPropertyBegin(fmt::format("Surface {}", surfaceIndex));

        table.rowReadOnlyInteger(
            "Index Count", static_cast<int32_t>(surface.indexCount)
        );

        uiAssetReadOnlyNameField(
            table, "Occlusion-Roughness-Metallic", surface.material.ORM
        );
        uiAssetReadOnlyNameField(table, "Normal", surface.material.normal);
        uiAssetReadOnlyNameField(table, "Color", surface.material.color);

        table.childPropertyEnd();
        surfaceIndex++;
    }

    table.childPropertyEnd();
}

void uiMeshMaterialOverrides(
    syzygy::PropertyTable& table,
    syzygy::MeshInstanced& instance,
    std::span<syzygy::AssetPtr<syzygy::ImageView> const> const textures
)
{
    table.rowChildPropertyBegin("Material Overrides");

    size_t surface{0};
    for (syzygy::MaterialData const& materialOverride :
         instance.getMaterialOverrides())
    {
        table.rowChildPropertyBegin(fmt::format("Surface {}", surface));

        bool changed{false};

        syzygy::MaterialData newOverride{materialOverride};

        table.rowCustom(
            "Occlusion-Roughness-Metallic",
            [&]()
        {
            auto newORM{uiAssetSelection<syzygy::ImageView>(
                syzygy::assetPtrToRef(newOverride.ORM), textures
            )};
            if (newORM.has_value())
            {
                newOverride.ORM = newORM.value();
                changed = true;
            }
        },
            newOverride.ORM.lock() != nullptr,
            [&]()
        {
            newOverride.ORM.reset();
            changed = true;
        }
        );
        table.rowCustom(
            "Normal",
            [&]()
        {
            auto newNormal{uiAssetSelection<syzygy::ImageView>(
                syzygy::assetPtrToRef(newOverride.normal), textures
            )};
            if (newNormal.has_value())
            {
                newOverride.normal = newNormal.value();
                changed = true;
            }
        },
            newOverride.normal.lock() != nullptr,
            [&]()
        {
            newOverride.normal.reset();
            changed = true;
        }
        );
        table.rowCustom(
            "Color",
            [&]()
        {
            auto newColor{uiAssetSelection<syzygy::ImageView>(
                syzygy::assetPtrToRef(newOverride.color), textures
            )};
            if (newColor.has_value())
            {
                newOverride.color = newColor.value();
                changed = true;
            }
        },
            newOverride.color.lock() != nullptr,
            [&]()
        {
            newOverride.color.reset();
            changed = true;
        }
        );

        if (changed)
        {
            instance.setMaterialOverrides(surface, newOverride);
        }

        surface++;

        table.childPropertyEnd();
    }

    table.childPropertyEnd();
}

void uiSceneGeometry(
    syzygy::AABB& bounds,
    std::span<syzygy::MeshInstanced> const geometry,
    std::span<syzygy::AssetPtr<syzygy::Mesh> const> const meshes,
    std::span<syzygy::AssetPtr<syzygy::ImageView> const> const textures
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
        table.rowBoolean("Casts Shadow", instance.castsShadow, true);

        {
            table.rowChildPropertyBegin("Transforms");
            for (size_t transformIndex{0};
                 transformIndex < std::min(
                     instance.transforms.size(), instance.originals.size()
                 );
                 transformIndex++)
            {
                uiTransform(
                    table,
                    instance.transforms[transformIndex],
                    instance.originals[transformIndex]
                );
            }
            table.childPropertyEnd();
        }

        table.rowCustom(
            "Instance Animation",
            [&]() { uiInstanceAnimation(instance.animation); }
        );
        table.rowCustom(
            "Mesh Used",
            [&]()
        {
            auto newMesh{
                uiAssetSelection<syzygy::Mesh>(instance.getMesh(), meshes)
            };
            if (newMesh.has_value())
            {
                instance.setMesh(newMesh.value());
            }
        }
        );
        if (auto instanceMeshRef{instance.getMesh()};
            instanceMeshRef.has_value())
        {
            table.childPropertyBegin(false);
            syzygy::Asset<syzygy::Mesh> const& meshAsset{
                instanceMeshRef.value().get()
            };
            uiAssetMetadata(table, meshAsset.metadata);
            if (meshAsset.data != nullptr)
            {
                uiMesh(table, *meshAsset.data);
            }
            uiMeshMaterialOverrides(table, instance, textures);

            table.childPropertyEnd();
        }

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
    std::span<AssetPtr<Mesh> const> const meshes,
    std::span<AssetPtr<ImageView> const> const textures
)
{
    UIWindowScope const window{
        UIWindowScope::beginDockable(std::format("{}##scene", title), dockNode)
    };
    if (!window.isOpen())
    {
        return;
    }

    if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen))
    {
        SceneTime const& defaultAnimation{Scene::DEFAULT_SUN_ANIMATION};

        FloatBounds constexpr SUN_ANIMATION_SPEED_BOUNDS{
            -100'000.0F, 100'000.0F
        };

        PropertySliderBehavior RADIANS_BEHAVIOR{
            .speed = 0.01F,
            .bounds =
                FloatBounds{.min = -glm::pi<float>(), .max = glm::pi<float>()}
        };

        auto table{PropertyTable::begin()};

        table.rowBoolean("Frozen", scene.time.frozen, defaultAnimation.frozen)
            .rowFloat(
                "Time (Days)",
                scene.time.time,
                defaultAnimation.time,
                PropertySliderBehavior{.speed = 0.01F}
            )
            .rowFloat(
                "Speed",
                scene.time.speed,
                defaultAnimation.speed,
                PropertySliderBehavior{
                    .bounds = SUN_ANIMATION_SPEED_BOUNDS,
                }
            )
            .rowBoolean("Realistic Orbits", scene.time.realisticOrbits, true)
            .childPropertyBegin()
            .rowFloat(
                "Planet Tilt (Radians)",
                scene.time.tiltPlanet,
                defaultAnimation.tiltPlanet,
                RADIANS_BEHAVIOR
            )
            .rowFloat(
                "Lunar Orbit Inclination (Radians)",
                scene.time.inclinationLunarOrbit,
                defaultAnimation.inclinationLunarOrbit,
                RADIANS_BEHAVIOR
            )
            .childPropertyEnd();

        if (scene.time.realisticOrbits)
        {
            table.rowReadOnlyBoolean("Skip Night", scene.time.skipNight);
        }
        else
        {
            table.rowBoolean(
                "Skip Night", scene.time.skipNight, defaultAnimation.skipNight
            );
        }

        table.end();
    }

    if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
    {
        uiAtmosphere(scene.atmosphere, Scene::DEFAULT_ATMOSPHERE_EARTH);
    }

    if (ImGui::CollapsingHeader(
            "Atmospheric Lights", ImGuiTreeNodeFlags_DefaultOpen
        ))
    {
        uiAtmosphereLights(scene.atmosphereLights());
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
        auto sceneBounds{scene.shadowBounds()};
        // uiSceneGeometry(sceneBounds, scene.geometry(), meshes, textures);
    }
}

auto sceneViewportWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    std::optional<UIRectangle> maximizeArea,
    ImTextureID const sceneTexture,
    ImVec2 const sceneTextureMax,
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

    UIWindowScope sceneViewport{
        maximizeArea.has_value()
            ? UIWindowScope::beginMaximized(title, maximizeArea.value())
            : UIWindowScope::beginDockable(title, dockNode)
    };

    if (!sceneViewport.isOpen())
    {
        return {
            .focused = false,
            .payload = std::nullopt,
        };
    }

    glm::vec2 const contentExtent{sceneViewport.screenRectangle().size()};

    ImVec2 const uvMax{contentExtent / glm::vec2{sceneTextureMax}};

    float const textHeight{
        ImGui::CalcTextSize("").y + ImGui::GetStyle().ItemSpacing.y
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{});
    bool const clicked = ImGui::ImageButton(
        "##viewport",
        sceneTexture,
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