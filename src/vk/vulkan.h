// One place that pulls in GLFW and the Vulkan headers, so every file includes them the same way.
#pragma once

// GLFW_INCLUDE_NONE stops GLFW from pulling in the OpenGL header (GL/gl.h),
// which this Vulkan-only renderer never uses. GLFW_INCLUDE_VULKAN still adds vulkan/vulkan.h.
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
