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

# Locate Vulkan SDK installed on host operating system
find_package(Vulkan REQUIRED)

# Define your main executable target
add_executable(Ray_Tracing_Renderer src/main.cpp)

# Link GLFW and Vulkan against your executable
target_link_libraries(Ray_Tracing_Renderer PRIVATE glfw Vulkan::Vulkan)

find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin")

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/raytrace.comp.spv
    COMMAND ${GLSLC} ${CMAKE_SOURCE_DIR}/shaders/raytrace.comp -o ${CMAKE_BINARY_DIR}/raytrace.comp.spv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/raytrace.comp 
    COMMENT "Compiling raytrace.comp"
)

add_custom_target(shaders DEPENDS ${CMAKE_BINARY_DIR}/raytrace.comp.spv)
add_dependencies(Ray_Tracing_Renderer shaders)