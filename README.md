# VulkanRenderer

This project requires the Vulkan SDK (at least 1.3.280.0).
I have not tested compilation with anything but Visual Studio 2022's built in CMake functionality, compiling with 64-bit MSVC.
Shader sources in GLSL are included, but so are the compiled SPIR-V equivalents which should be up to date with every commit. There is an optional CMake build target `Shaders` that uses glslangValidator, which comes with the Vulkan SDK. If you change any shader files, run that target. If you add or remove any shader files, rerun CMake so it can update the target.