# VulkanRenderer

The purpose of this renderer is to study concepts in rendering, engine architecture, and modern Vulkan. In order to aid learning and demonstration, the engine is designed with a focus in configurability and exposing the internals of the rendering.

## Requirements

This project was developed in Visual Studio Community 2022 on Windows 11, and compiled with `msvc_x64`.\
Requires CMake 3.28 or higher, and a version of Visual Studio that has CMake support.\
Running the engine currently requires a GPU with drivers that support Vulkan 1.3 and the device extension `VK_EXT_shader_object` (and a few other features/extensions).

## Dependencies

- [Vulkan SDK](https://vulkan.lunarg.com/), at least 1.3.280, for `vulkan.h`, `glslangValidator.exe`, and a few debug utilities

CMake is configured to use FetchContent to pull most of the following dependencies from Github. See [`cmake/dependencies.cmake`](cmake/dependencies.cmake) for the most up to date versions.

- [Dear ImGui](https://github.com/ocornut/imgui)
- [fastgltf](https://github.com/spnda/fastgltf.git)
- [fmt](https://github.com/fmtlib/fmt.git)
- [glfw](https://github.com/glfw/glfw.git)
- [glm](https://github.com/g-truc/glm.git)
- [implot](https://github.com/epezent/implot), a library which extends Dear ImGui to add real-time plots
- [spirv-reflect](https://github.com/KhronosGroup/SPIRV-Reflect.git)
- [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap.git)
- [volk](https://github.com/zeux/volk.git)
- [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git)

CMake is configured with an optional `Shaders` target, which calls upon `glslangValidator` to compile all the shaders (with extensions `.frag`, `.vert`, `.comp`) in the [`shaders`](shaders) folders. The repo will include the most up-to-date versions of the compiled `.spv` files, so this step is only needed to aid development.

## Building

Pull the repo with:

```bash
git clone https://github.com/EllarBooher/VulkanRenderer.git
```

Open the root folder with Visual Studio, configure the CMake cache (by default `Project -> Configure Cache`), set the build target to `Renderer.exe`, and build.

## Showcase

![image](assets/screenshots/deferred_sunset.png)
*Pictured above is an implementation (see [`shaders/deferred/sky.comp`](shaders/deferred/sky.comp)) of a volume rendering model of light scattering in the sky, alongside deferred-shaded directional lights and spot lights*

![image](assets/screenshots/deferred_night.png)
*Here is the above scene, but at night and from a different angle*

## Features

- A deferred shading pipeline with multiple passes, including post-process
- Dynamic sun driving volume rendering model of light scattering and the directional lighting for the scene
- Runtime reflection of SPIR-V shaders for data verification, easier resource management, and to populate the UI
- Utilizes modern Vulkan features such as Dynamic Rendering, Shader Objects, and Bindless Design via Buffer References and Runtime Descriptor Arrays

## Planned Features

- Better UI and logging
- Collection of runtime information from scenes/pipelines
- Physically based rendering
- Asset management
- Render graph
- Rigidbody physics engine
- Dynamic scenes with controllable transformations
- Fallbacks for extensions and rendering features (for example, `VK_EXT_shader_object` has low coverage)
- Runtime compilation and modification of shader code

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
