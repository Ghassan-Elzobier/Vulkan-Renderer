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

// A buffer and the memory behind it. The harness uses a host-visible one to read the image back.
struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
};

Buffer createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
void destroyBuffer(VkDevice device, const Buffer& buffer);
