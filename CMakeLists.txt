cmake_minimum_required(VERSION 3.20)
project(Ray_Tracing_Renderer LANGUAGES CXX)

# Globally enforces C++ for all targets in this directory or subdirectory
# Compile code with C++ standard
set(CMAKE_CXX_STANDARD 20)
# Make C++20 a hard requirement
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Strict ISO C++ standard rather than compiler specific extensions. More reliable cross platform portability.
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake's built in module to download configure and populate external dependencies at configure time
include(FetchContent)

# Disables unnecessary targets to fetch from the glfw repo
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Declare the GLFW repo and its version
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4 
)

# Download the repo and make it available as a target
FetchContent_MakeAvailable(glfw)

# stb: single-header image libraries, used to write and read PNG files. It has no CMake
# build of its own, so FetchContent only downloads it and we add its folder to the include path.
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 9600522de53973336fa033a289858a4354cbb04c
)
FetchContent_MakeAvailable(stb)

# Locate Vulkan SDK installed on host operating system
find_package(Vulkan REQUIRED)

# The scene on the CPU: meshes, OBJ loading and the BVH build. Plain C++, no Vulkan,
# so the BVH can be built and checked on any machine.
add_library(scene STATIC
    src/scene/mesh.cpp
    src/scene/bvh.cpp
    src/scene/sah_split.cpp
)
target_include_directories(scene PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Everything except main() is built once as a library, so later executables
# (the compute harness, benchmarks, tests) can link the same code.
add_library(renderer_core STATIC
    src/vk/context.cpp
    src/vk/swapchain.cpp
    src/vk/sync.cpp
    src/vk/resources.cpp
    src/vk/pipeline.cpp
    src/render/frame.cpp
    src/render/scene_buffers.cpp
    src/bench/gpu_timer.cpp
)
# Lets any file write #include "vk/context.h", relative to src/
target_include_directories(renderer_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
# PUBLIC: anything that links renderer_core also gets GLFW and Vulkan
target_link_libraries(renderer_core PUBLIC glfw Vulkan::Vulkan scene)

# Image saving, loading and comparing: plain C++, no Vulkan, so tools can use it without a GPU.
add_library(image_io STATIC
    src/image/image_io.cpp
    src/image/compare.cpp
)
target_include_directories(image_io PUBLIC ${CMAKE_SOURCE_DIR}/src PRIVATE ${stb_SOURCE_DIR})

# The app itself: main() wires the pieces together
add_executable(Ray_Tracing_Renderer src/main.cpp)
target_link_libraries(Ray_Tracing_Renderer PRIVATE renderer_core)

# The windowless compute harness: same path tracer and timing, no window, saves the image.
add_executable(Ray_Tracing_Harness src/harness/main.cpp)
target_link_libraries(Ray_Tracing_Harness PRIVATE renderer_core image_io)

# Compares a render with a reference image and reports the RMSE. Needs no GPU.
add_executable(Image_Compare tools/image_compare.cpp)
target_link_libraries(Image_Compare PRIVATE image_io)

# Builds BVHs for a few meshes and checks they find the same closest hit as brute force for
# many random rays, then prints the tree's statistics. Needs no GPU. Run it with ctest.
add_executable(Bvh_Check tools/bvh_check.cpp)
target_link_libraries(Bvh_Check PRIVATE scene)
enable_testing()
add_test(NAME bvh_matches_brute_force COMMAND Bvh_Check)

find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin")

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/raytrace.comp.spv
    COMMAND ${GLSLC} ${CMAKE_SOURCE_DIR}/shaders/raytrace.comp -o ${CMAKE_BINARY_DIR}/raytrace.comp.spv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/raytrace.comp 
    COMMENT "Compiling raytrace.comp"
)

add_custom_target(shaders DEPENDS ${CMAKE_BINARY_DIR}/raytrace.comp.spv)
add_dependencies(Ray_Tracing_Renderer shaders)
add_dependencies(Ray_Tracing_Harness shaders)