// GPU memory the shader reads and writes. Today that is the two storage images;
// the scene and BVH buffers will live here too.
#pragma once

#include "vk/vulkan.h"

#include <cstdint>

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

// One storage image is three objects: the image, its device-local memory and a view.
struct StorageImage {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
};

StorageImage createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat imageFormat);
void destroyStorageImage(VkDevice device, const StorageImage& storageImage);
