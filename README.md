# Woven Core 🕸️

![Language](https://img.shields.io/badge/C++-23-blue.svg)
![API](https://img.shields.io/badge/Vulkan-1.4%2B-red.svg)
![Platform](https://img.shields.io/badge/Windows-Linux-lightgrey.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

This project is for my learning and experimentation with modern graphics programming techniques. It is not intended for production use or as a full game engine. The focus is on exploring the latest Vulkan features and best practices not making a full game engine.

**Woven Core** will try follow the **2026 Roadmap** specification as best as possible.

## 🚀 The 2026 Tech Stack

| Component | Technology | Reasoning |
| :--- | :--- | :--- |
| **Windowing** | **SDL3** | Superior high-DPI support and modern event loop compared to GLFW. |
| **Graphics API** | **Vulkan 1.4** | Using `VK_EXT_shader_object` and `VK_KHR_dynamic_rendering`. |
| **Vulkan Setup** | **vk-bootstrap** | Simplifies instance, device, and queue creation with error handling. |
| **Vulkan Loader** | **Volk** | Meta-loader for Vulkan with per-device function pointers. |
| **GPU Memory** | **VMA** | Industry standard for GPU memory virtualization. |
| **Assets** | **fastgltf** | C++20 SIMD-optimized glTF loader (20x faster than tinygltf). |
| **Physics** | **Jolt Physics** | Multi-threaded, double-precision physics engine used in AAA. |
| **Tasks** | **enkiTS** | Lightweight, lock-free task scheduler for job-based parallelism. |
| **Profiling** | **Tracy v0.13.1** | Real-time frame profiler with GPU support and memory tracking. |
| **Math** | **GLM** | Configured for Left-Handed, Zero-to-One depth (Vulkan standard). |
| **Build System** | **CMake + CPM** | Package manager included; no manual git submodules required. |

## 🛠️ Prerequisites

To build Woven Core, you need:

1.  **Vulkan SDK** (Latest Version): [Download Here](https://vulkan.lunarg.com/)
2.  **C++ Compiler**:
    *   **Windows**: MSVC (Visual Studio 2022 v17.8+) or Clang-CL.
    *   **Linux**: GCC 13+ or Clang 16+ (Must support C++23).
3.  **CMake**: Version 3.28 or higher.

## 🏗️ Building and Running

We use **CMake Presets** to simplify building across platforms.

### VS Code (Recommended)
1.  Install **CMake Tools** extension.
2.  Open the folder.
3.  **Configure:** Press `Ctrl+Shift+P` -> **CMake: Select Configure Preset**.
    *   Windows: Select `windows-vs2022`.
    *   Linux: Select `linux-debug` or `linux-release`.
4.  **Build:** Press **F7** (or click Build in status bar).
5.  **Run:** Press **F5**.

### Command Line

**1. Configuration (Generate Project Files)**
Run this once (or when you add new files).

```bash
# Windows (Generates .sln for Visual Studio)
cmake --preset windows-vs2022

# Linux (Generates Ninja files)
cmake --preset linux-debug
# OR for Release:
cmake --preset linux-release
```

Building (Compile)

```bash
# Windows - Debug
cmake --build --preset windows-debug

# Windows - Release
cmake --build --preset windows-release

# Linux
cmake --build --preset linux-debug
# OR
cmake --build --preset linux-release
```

## 📦 Packaging for Distribution

To create a standalone .zip (Windows) or .tar.gz/.deb (Linux) containing the engine and all assets:

```bash
# Windows
cd build/windows-vs
cpack -C Release

# Linux
cd build/linux-release
cpack -G TGZ
```

Artifacts will be generated in the build directory.

## 📂 Project Structure

```code
WovenCore/
├── CMakeLists.txt       # Main build script
├── CMakePresets.json    # Build profiles (Debug/Release/OS)
├── src/
│   └── main.cpp         # Engine Entry Point
├── shaders/             # HLSL/GLSL/Slang Shaders
└── assets/              # Models and Textures (copied to bin/ on build)
```

## 🧠 Architecture Highlights

Bindless Resources: Textures are accessed via indices into a global descriptor heap (NonUniformEXT). WIP

No Render Passes: We use vkCmdBeginRendering exclusively. WIP

Task Graph: Logic updates and Physics (Jolt) run on background threads via enkiTS. WIP

Static Linking: Dependencies like SDL3 and Jolt are statically linked to reduce DLL hell. WIP

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.
