// The Vulkan objects everything else is built from: the instance, the debug
// messenger, the chosen GPU and its queue families, and the logical device.
#pragma once

#include "vk/vulkan.h"

#include <cstdint>
#include <optional>
#include <vector>

#ifdef NDEBUG // Disable validation in production build
    constexpr bool enableValidationLayers = false;
#else // enable validation in debugging build
    constexpr bool enableValidationLayers = true;
#endif

struct QueueFamilyIndices {
    std::optional<uint32_t> compute;
    std::optional<uint32_t> present;
};

// windowExtensions are the instance extensions a window needs (from glfwGetRequiredInstanceExtensions).
// A windowless run passes none, so this module never asks GLFW for anything.
VkInstance createInstance(const std::vector<const char*>& windowExtensions);
VkDebugUtilsMessengerEXT setupDebugMessenger(VkInstance instance);
void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger);

// surface may be VK_NULL_HANDLE for a windowless run: then no present queue or
// swapchain extension is looked for, and indices.present stays empty.
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
// presenting = true enables the swapchain extension; the windowless harness passes false.
VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, bool presenting);
