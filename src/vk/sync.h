// The per-frame tools: where commands are recorded, how the CPU and GPU wait for each other,
// and the timestamp queries the benchmark reads.
#pragma once

#include "vk/vulkan.h"

#include <cstdint>

VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamilyIndex);
VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool commandPool);
VkSemaphore createSemaphore(VkDevice device);
// Created signalled, so the first wait on it returns at once.
VkFence createFence(VkDevice device);
// Two timestamps: before and after the dispatch.
VkQueryPool createQueryPool(VkDevice device);
