# Syzygy

Syzygy is an engine and renderer I started to study concepts in rendering, engine architecture, and modern Vulkan.

This project was developed in Visual Studio Community 2022 on Windows 11. It has been compiled with both MSVC (`cl.exe`) and Clang (`clang-cl.exe`) via VS integration.

## Requirements

For now, only Windows is supported, and compilation requires CMake version 3.28 or higher.

Running the engine currently requires a GPU with drivers that supports at least version `1.3.280` of the Vulkan API. The engine relies on the device extension `VK_EXT_shader_object`, and likely a few others. `VK_EXT_shader_object` does not have wide adoption (as of 2024). The engine is currently not compatible with the emulation layer `VK_LAYER_KHRONOS_shader_object`, but this will likely be remedied later on.

## Dependencies

You must download the following, or figure out a way to provide the required files yourself:

- [Vulkan SDK](https://vulkan.lunarg.com/), at least 1.3.280, for `vulkan.h`, `glslangValidator.exe`, and a few debug utilities

CMake is configured to use FetchContent to pull all of the following dependencies from Github. See [`cmake/dependencies.cmake`](cmake/dependencies.cmake) for the versions in use.

- [fastgltf](https://github.com/spnda/fastgltf.git), for loading 3D models and scenes
- [spdlog](https://github.com/gabime/spdlog.git), for logging
- [fmt](https://github.com/fmtlib/fmt.git), for formatting strings. This project uses the version bundled with spdlog.
- [glfw](https://github.com/glfw/glfw.git), for the windowing backend
- [glm](https://github.com/g-truc/glm.git), for linear algebra
- [Dear ImGui](https://github.com/ocornut/imgui), for the user interface
- [implot](https://github.com/epezent/implot), for real-time plots in Dear ImGui
- [spirv-reflect](https://github.com/KhronosGroup/SPIRV-Reflect.git), for reflecting SPIR-V shader bytecode
- [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap.git), for the initialization of some Vulkan objects
- [volk](https://github.com/zeux/volk.git), for dynamically linking to Vulkan
- [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git), for quickly allocating memory via Vulkan

## Building

First clone the repo:

```bash
git clone https://github.com/EllarBooher/Syzygy.git
```

To configure and build:

1. Open the root folder that git downloaded, directly with Visual Studio.
2. Select a build configuration. In order to see their definitions, read [`CMakePresets.json`](CMakePresets.json). To add your own local configurations, create [`CMakeUserPresets.json`](CMakeUserPresets.json). See the official CMake documentation on [`cmake-presets`](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) for more information.
3. Run CMake via `Project -> Configure Cache`. Swapping to the desired build configuration should trigger this process by default. This step may take a while, as CMake needs to download the dependencies.
4. After CMake has finished running, you can build and run `Syzygy.exe`.

Alternatively, you can always run cmake yourself. For example, run the following from a folder outside of the source:

```bash
cmake path/including/Syzygy -G "Visual Studio 17 2022"
```

Some notes on building:

- If you have [include-what-you-use](https://github.com/include-what-you-use/include-what-you-use) installed, there is a CMake cache variable `IWYU_ENABLE` to run it alongside compilation.
- `clang-format` and `clang-tidy` are used to enforce coding standards in this project. `clang-format` can be ran via a utility target, while `clang-tidy` is enabled with `CLANG_TIDY_ENABLE` to run it through CMake's [`CMAKE_CXX_CLANG_TIDY`](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_CLANG_TIDY.html) target property.
- CMake is set up to `find_program` for each of these tools, but you can also set their respective `[TOOL NAME]_PATH` cache variable to override the location.

## Showcase

![image](assets/screenshots/sunset_maximized.png)
*Pictured above is a volume rendering model of light scattering in the sky, alongside deferred-shaded lighting. See [`shaders/deferred/sky.comp`](shaders/deferred/sky.comp) for the implementation of the sky's compute pass, which tints every pixel based on atmospheric scattering.*

![image](assets/screenshots/teal_day.png)
*The user interface, which utilizes Dear ImGui's docking features to allow dragging, dropping, and resizing*

## Features

- A deferred shading pipeline with multiple passes, including post-process
- A dynamic sun with passing time, which drives a volume rendering model of light scattering
- Free flying camera controlled by mouse and keyboard
- Directional and spot lights
- Runtime reflection of SPIR-V shaders for data verification, easier resource management, and to populate the UI
- Utilizes modern Vulkan features such as Dynamic Rendering, Shader Objects, and Bindless Design via Buffer References and Runtime Descriptor Arrays

## Planned Features

- Serialization for UI and saving
- Dynamic scenes with controllable transformations
- Portability of the engine, both for source compilation and required GPU features
- Runtime metrics and benchmarking
- Physically based rendering
- Asset management
- Render graph
- Rigidbody physics engine
- Runtime compilation and modification of shader code
- Multithreading
- Release builds

## Resources

These are resources and other projects referred to in the development of Syzygy so far.

- [Vulkan Guide](https://vkguide.dev/)
- [Scratchapixel 4.0](https://www.scratchapixel.com/index.html)
- [Vulkan specification](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html)
- [OpenGL wiki](https://www.khronos.org/opengl/wiki/Core_Language_(GLSL))
- [SPIR-V specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)
- [Sascha Willems's repository of Vulkan examples](https://github.com/SaschaWillems/Vulkan)
- The [Vulkan Renderer](https://github.com/inexorgame/vulkan-renderer) for [Inexor](https://inexor.org/)
- The [Hazel](https://github.com/TheCherno/Hazel) engine
