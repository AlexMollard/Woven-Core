cmake_minimum_required(VERSION 3.28)
include(GNUInstallDirs) # Standard directory layout handling

# --- Project Configuration ---
project(WovenCore 
    VERSION 1.0.0
    DESCRIPTION "High-performance Vulkan Renderer (2026 Stack)"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Optimization: Interprocedural Optimization (LTO) ---
# Critical for renderers: allows inlining across translation units
include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)
if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    message(STATUS "IPO/LTO Enabled for Release Builds")
else()
    message(WARNING "IPO/LTO not supported: ${output}")
endif()

# --- Dependency Manager (CPM) ---
file(DOWNLOAD
     https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.42.1/CPM.cmake
     ${CMAKE_BINARY_DIR}/cmake/CPM.cmake)
include(${CMAKE_BINARY_DIR}/cmake/CPM.cmake)

# Handle FetchContent deprecation warnings
cmake_policy(SET CMP0169 OLD)

# --- Dependencies ---

# 1. SDL3 (Window/Input)
CPMAddPackage(
    NAME SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.2
    OPTIONS "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_TEST_LIBRARY OFF" "SDL_DISABLE_INSTALL ON"
)

# Ensure SDL3 target exists
if(NOT TARGET SDL3::SDL3-static AND TARGET SDL3-static)
    add_library(SDL3::SDL3-static ALIAS SDL3-static)
endif()

# 2. Volk (Vulkan Loader)
CPMAddPackage(
    NAME volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG master
    OPTIONS "VOLK_INSTALL OFF" "VOLK_PULL_IN_VULKAN ON"
)

# Ensure volk target exists
if(NOT TARGET Volk::volk)
    add_library(Volk::volk INTERFACE IMPORTED)
    target_include_directories(Volk::volk INTERFACE ${volk_SOURCE_DIR})
    target_link_libraries(Volk::volk INTERFACE Vulkan::Vulkan)
endif()

# 3. VMA (Memory)
CPMAddPackage(
    NAME VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG master
    OPTIONS "VMA_STATIC_VULKAN_FUNCTIONS OFF" "VMA_DYNAMIC_VULKAN_FUNCTIONS ON"
)

# Ensure VMA target exists
if(NOT TARGET GPUOpen::VulkanMemoryAllocator)
    add_library(GPUOpen::VulkanMemoryAllocator INTERFACE IMPORTED)
    target_include_directories(GPUOpen::VulkanMemoryAllocator INTERFACE ${VulkanMemoryAllocator_SOURCE_DIR}/include)
    target_link_libraries(GPUOpen::VulkanMemoryAllocator INTERFACE Vulkan::Vulkan)
endif()

# 4. GLM (Math)
CPMAddPackage(
    NAME glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG master
    OPTIONS "GLM_TEST_ENABLE OFF"
)

# Ensure GLM target exists
if(NOT TARGET glm::glm AND TARGET glm)
    add_library(glm::glm ALIAS glm)
endif()

# 5. fastgltf (Assets)
CPMAddPackage(
    NAME fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG v0.9.0
    OPTIONS "FASTGLTF_ENABLE_TESTS OFF"
)

# Ensure fastgltf target exists
if(NOT TARGET fastgltf::fastgltf AND TARGET fastgltf)
    add_library(fastgltf::fastgltf ALIAS fastgltf)
endif()

# 6. enkiTS (Tasks)
CPMAddPackage(
    NAME enkiTS
    GIT_REPOSITORY https://github.com/dougbinks/enkiTS.git
    GIT_TAG master
    OPTIONS "ENKITS_INSTALL OFF" "ENKITS_BUILD_EXAMPLES OFF"
)

# Ensure enkiTS target exists
if(NOT TARGET enkiTS::enkiTS AND TARGET enkiTS)
    add_library(enkiTS::enkiTS ALIAS enkiTS)
endif()

# 7. Jolt Physics
set(USE_SSE4_2 ON CACHE BOOL "Force SSE4.2" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use dynamic runtime library" FORCE)
CPMAddPackage(
    NAME JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG master
    SOURCE_SUBDIR Build
)

# Ensure Jolt has proper include directories exposed
if(TARGET Jolt)
    get_target_property(JOLT_INCLUDES Jolt INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT JOLT_INCLUDES)
        target_include_directories(Jolt PUBLIC ${JoltPhysics_SOURCE_DIR})
    endif()
endif()

find_package(Vulkan REQUIRED)

# 8. vk-bootstrap (Vulkan Initialization Helper)
CPMAddPackage(
    NAME vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG main
)

# Ensure vk-bootstrap target exists
if(NOT TARGET vk-bootstrap::vk-bootstrap AND TARGET vk-bootstrap)
    add_library(vk-bootstrap::vk-bootstrap ALIAS vk-bootstrap)
endif()

# 9. Tracy Profiler (Full Integration: CPU, GPU, Memory, Callstacks)
CPMAddPackage(
    NAME tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.13.1
    OPTIONS
        "TRACY_ENABLE ON"
        "TRACY_ON_DEMAND OFF"
        "TRACY_NO_BROADCAST OFF"
        "TRACY_ONLY_LOCALHOST OFF"
        "TRACY_ONLY_IPV4 OFF"
        "TRACY_CALLSTACK ON"           # Enable call stack capture
        "TRACY_NO_FRAME_IMAGE OFF"     # Allow frame capture
        "TRACY_NO_SAMPLING OFF"        # Enable sampling profiler
)

# --- Executable Target ---
# Glob source files (CONFIGURE_DEPENDS ensures CMake reconfigures when files are added/removed)
file(GLOB_RECURSE WOVENCORE_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.hpp"
    "src/*.h"
    "src/*/*.cpp"
    "src/*/*.hpp"
    "src/*/*.h"
)

add_executable(WovenCore ${WOVENCORE_SOURCES})

# Configure include directories and compile settings
target_include_directories(WovenCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${JoltPhysics_SOURCE_DIR}
)

# Enable Precompiled Headers AFTER include directories are set
target_precompile_headers(WovenCore PRIVATE src/pch.hpp)

target_compile_definitions(WovenCore PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_LEFT_HANDED
    GLM_FORCE_RADIANS
    JPH_PROFILE_ENABLED
    TRACY_ENABLE              # Enable Tracy profiling
    TRACY_CALLSTACK=8         # Capture 8 levels of callstack
    TRACY_VK_USE_SYMBOL_TABLE # Enable Vulkan symbol resolution
)

# Disable Jolt Debug Renderer in Release to save size/perf
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(WovenCore PRIVATE JPH_DEBUG_RENDERER=0)
else()
    target_compile_definitions(WovenCore PRIVATE JPH_DEBUG_RENDERER=1)
    # Enhanced debug info for Tracy callstack resolution
    if(MSVC)
        target_compile_options(WovenCore PRIVATE 
            /Zi          # Full debug info
            /Ob0         # Disable inline expansion for better callstacks
            /Od          # Disable optimizations
        )
        target_link_options(WovenCore PRIVATE /DEBUG:FULL)
    endif()
endif()

target_link_libraries(WovenCore PRIVATE
    SDL3::SDL3-static
    Volk::volk
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    glm::glm
    fastgltf::fastgltf
    enkiTS::enkiTS
    Jolt
    TracyClient
    Vulkan::Vulkan
)

target_compile_definitions(WovenCore PRIVATE
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
    VMA_STATIC_VULKAN_FUNCTIONS=0
)

# --- Installation Rules (Structuring the Release) ---

# 1. Install the Executable
install(TARGETS WovenCore
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 2. Install Assets (Shaders, Textures, Models)
# Assumes you have a folder named 'assets' and 'shaders' in your source root
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/shaders")
    # In a real engine, you would compile these to SPIR-V here before installing.
    # For now, we install the raw files.
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders" DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# 3. Handle System DLLs (Windows Only)
# If using shared libs (like Slang or generic Vulkan layers), copying them is needed.
# Since we static link almost everything, we just need to ensure the user has Drivers/Redist.

# --- Packaging (CPack) ---
set(CPACK_PACKAGE_VENDOR "WovenTechnologies")
set(CPACK_PACKAGE_NAME "WovenCore-Engine")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Next-Gen Vulkan Renderer")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE") # Ensure you have a LICENSE file

# Generator Selection
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS") # Creates a .zip AND an .exe installer
    set(CPACK_NSIS_MODIFY_PATH ON)
elseif(UNIX)
    set(CPACK_GENERATOR "TGZ;DEB") # Creates a .tar.gz AND a .deb file
endif()

# This must be at the very end
include(CPack)