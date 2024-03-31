# VulkanRenderer

This project does not explicitly require the Vulkan SDK to build, but it is probably the easiest way to get the dependencies. This project uses Volk, so installing Vulkan however Volk likes to get it is what you need.
I have not tested compilation with anything but Visual Studio 2022's built in CMake functionality, compiling with 64-bit MSVC.
Shader sources in GLSL are included, but so are the compiled SPIR-V equivalents which should be up to date with every commit. There is an optional CMake build target `Shaders` that uses glslangValidator, which comes with the Vulkan SDK. If you change any shader files, run that target. If you add or remove any shader files, rerun CMake so it can update the target.