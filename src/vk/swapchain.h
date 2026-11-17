// The display's images: choose a format, present mode and size for the window, then create the swapchain.
// Everything here is rebuilt when the window size changes.
#pragma once

#include "vk/vulkan.h"
#include "vk/context.h"

#include <vector>

struct Swapchain {
    VkSwapchainKHR handle;
    std::vector<VkImage> images;
    VkFormat format;
    VkExtent2D extent;
};

Swapchain createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice,
                          VkSurfaceKHR surface, GLFWwindow* window,
                          const QueueFamilyIndices& indices);
void destroySwapchain(VkDevice device, const Swapchain& swapchain);
