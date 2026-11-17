// The benchmark's timing method, written once so every later benchmark measures the same way:
// skip the warm-up frames, then time the dispatch over the measured frames and report min, avg, max and Mpaths/s.
#pragma once

#include "vk/vulkan.h"

#include <cstdint>

struct GpuTimer {
    uint32_t warmupFrames = 100;
    uint32_t measureFrames = 1000;
    double totalMs = 0.0, minMs = 1e9, maxMs = 0.0;
    uint32_t measured = 0;
};

// Reads the two timestamps the previous frame wrote, frameIndex being that frame's number.
// Returns true once the last measured frame has been read.
bool readFrameTime(GpuTimer& timer, VkDevice device, VkQueryPool timestamps,
                   float timestampPeriod, uint32_t frameIndex);

void printReport(const GpuTimer& timer, const char* deviceName, VkExtent2D extent);
