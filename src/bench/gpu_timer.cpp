#include "bench/gpu_timer.h"

#include <algorithm>
#include <iostream>

bool readFrameTime(GpuTimer& timer, VkDevice device, VkQueryPool timestamps,
                   float timestampPeriod, uint32_t frameIndex) {
    uint64_t results[2];
    VkResult r = vkGetQueryPoolResults(device, timestamps, 0, 2, sizeof(results), results, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    if (r != VK_SUCCESS) {
        std::cerr << "query read failed: " << r << "\n";
        return false;
    }
    if (frameIndex < timer.warmupFrames) {
        return false;
    }

    double ms = (results[1] - results[0]) * timestampPeriod / 1e6;
    timer.totalMs += ms;
    timer.minMs = std::min(timer.minMs, ms);
    timer.maxMs = std::max(timer.maxMs, ms);
    timer.measured++;

    return timer.measured == timer.measureFrames;
}

void printReport(const GpuTimer& timer, const char* deviceName, VkExtent2D extent) {
    double avgMs = timer.totalMs / timer.measured;
    double pathsPerSec = double(extent.width) * extent.height / (avgMs / 1000.0);
    std::cout << "GPU: " << deviceName << "\n"
    << "Frames: " << timer.measured << " (1 spp each, max 8 bounces)\n"
    << "Dispatch avg " << avgMs << " ms, min" << timer.minMs << " ms, max" << timer.maxMs << " ms\n"
    << "Mpaths/s: " << pathsPerSec / 1e6 << "\n";
}
