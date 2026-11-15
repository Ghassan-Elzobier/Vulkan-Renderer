// The Vulkan objects everything else is built from: the instance, the debug
// messenger, the chosen GPU and its queue families, and the logical device.
#pragma once

#include "vk/vulkan.h"

#include <cstdint>
#include <optional>

#ifdef NDEBUG // Disable validation in production build
    constexpr bool enableValidationLayers = false;
#else // enable validation in debugging build
    constexpr bool enableValidationLayers = true;
#endif

struct QueueFamilyIndices {
    std::optional<uint32_t> compute;
    std::optional<uint32_t> present;
};

VkInstance createInstance();
VkDebugUtilsMessengerEXT setupDebugMessenger(VkInstance instance);
void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger);

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);
