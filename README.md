# Syzygy

The purpose of this renderer is to study concepts in rendering, engine architecture, and modern Vulkan. In order to aid learning and demonstration, the engine is designed with a focus in configurability and exposing the internals of the rendering.

## Requirements

This project was developed in Visual Studio Community 2022 on Windows 11. It has been compiled with both MSVC (`cl.exe`) and Clang (`clang-cl.exe`) via VS integration.

For now, only Windows is known to be supported.

Requires CMake 3.28 or higher.

Running the engine currently requires a GPU with drivers that support Vulkan 1.3 and the device extensions such as `VK_EXT_shader_object` and others. Note, this fact seems to stop RenderDoc from working. NSight complains about the newer extensions, but seems to still mostly work.

## Dependencies
You must download the following, or figure out a way to provide the required files yourself:

- [Vulkan SDK](https://vulkan.lunarg.com/), at least 1.3.280, for `vulkan.h`, `glslangValidator.exe`, and a few debug utilities

CMake is configured to use FetchContent to pull most of the following dependencies from Github. See [`cmake/dependencies.cmake`](cmake/dependencies.cmake) for the versions in use.

- [fastgltf](https://github.com/spnda/fastgltf.git), for loading 3D models and scenes
- [fmt](https://github.com/fmtlib/fmt.git), for formatting strings
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
- If you have [include-what-you-use](https://github.com/include-what-you-use/include-what-you-use) installed, there is a CMake cache variable `IWYU_ENABLE` to run it alongside compilation. You can specify a path via `IWYU_PATH`, or let CMake `find_program` get it.
- `clang-format` and `clang-tidy` are used to enforce coding standards in this project. `clang-format` is configured to run with an optional build target, while `clang-tidy` has a CMake cache variable `CLANG_TIDY_ENABLE` to integrate it with compilation.
- Due to how heavily they impact compilation time, these options are disabled by default.

## Showcase

![image](assets/screenshots/deferred_sunset.png)
*Pictured above is an implementation (see [`shaders/deferred/sky.comp`](shaders/deferred/sky.comp)) of a volume rendering model of light scattering in the sky, alongside deferred-shaded directional lights and spot lights*

![image](assets/screenshots/deferred_night.png)
*Here is the above scene, but at night and from a different angle.*

![image](assets/screenshots/interface.png)
*The user interface, which utilizes Dear ImGui's docking features to allow dragging, dropping, and resizing*

## Features

- A deferred shading pipeline with multiple passes, including post-process
- A dynamic sun, which drives a volume rendering model of light scattering
- Directional and spot lights
- Runtime reflection of SPIR-V shaders for data verification, easier resource management, and to populate the UI
- Utilizes modern Vulkan features such as Dynamic Rendering, Shader Objects, and Bindless Design via Buffer References and Runtime Descriptor Arrays

## Planned Features

- Serialization for UI and saving
- Dynamic scenes with controllable transformations
- Logging with categories, saving to file, and quality of life features such as deduplication
- Portability of the engine, both for source compilation and required GPU features
- Runtime metrics and benchmarking
- Physically based rendering
- Asset management
- Render graph
- Rigidbody physics engine
- Runtime compilation and modification of shader code
- Multithreading

## Resources

These are resources and other projects referred to in the development of this project so far.

- [Vulkan Guide](https://vkguide.dev/)
- [Scratchapixel 4.0](https://www.scratchapixel.com/index.html)
- [Vulkan specification](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html)
- [OpenGL wiki](https://www.khronos.org/opengl/wiki/Core_Language_(GLSL))
- [SPIR-V specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)
- [Sascha Willems's repository of Vulkan examples](https://github.com/SaschaWillems/Vulkan)
- The [Vulkan Renderer](https://github.com/inexorgame/vulkan-renderer) for [Inexor](https://inexor.org/)
- The [Hazel](https://github.com/TheCherno/Hazel) engine
