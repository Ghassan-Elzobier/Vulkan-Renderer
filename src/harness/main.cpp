#include "vk/vulkan.h"
#include "vk/context.h"
#include "vk/sync.h"
#include "vk/resources.h"
#include "vk/pipeline.h"
#include "render/frame.h"
#include "bench/gpu_timer.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// The windowless compute harness: the same path tracer and the same timing as the window,
// with no surface, swapchain or present semaphores. It runs the warm-up and measured frames,
// prints the benchmark report, then reads the output image back and saves it as a PPM file.
//
// Usage: Ray_Tracing_Harness [--out file.ppm] [--size WIDTHxHEIGHT]

namespace {

struct Options {
    std::string outPath = "harness.ppm";
    VkExtent2D extent{800, 600}; // the window's size, so the numbers compare like with like
};

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) {
            options.outPath = argv[++i];
        } else if (arg == "--size" && i + 1 < argc) {
            unsigned w = 0, h = 0;
            if (std::sscanf(argv[++i], "%ux%u", &w, &h) != 2 || w == 0 || h == 0) {
                throw std::runtime_error("--size wants WIDTHxHEIGHT, got '" + std::string(argv[i]) + "'");
            }
            options.extent = {w, h};
        } else {
            throw std::runtime_error("Unknown argument '" + arg + "'");
        }
    }
    return options;
}

// Submits one command buffer and waits on the fence for it to finish.
void submitAndWait(VkDevice device, VkQueue queue, VkCommandBuffer commandBuffer, VkFence fence) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkResetFences(device, 1, &fence);
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
}

// Copies the output image into a host-visible buffer, so the CPU can read its bytes.
// The image is in GENERAL after the last trace; it moves to TRANSFER_SRC for the copy.
void recordReadback(VkCommandBuffer commandBuffer, VkImage image, VkExtent2D extent, VkBuffer buffer) {
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.image = image;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransfer.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    // bufferRowLength and bufferImageHeight of 0 mean "tightly packed": width * 4 bytes per row.
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

    // The copy's writes must be visible to the CPU before it maps the buffer.
    VkBufferMemoryBarrier toHost{};
    toHost.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    toHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    toHost.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toHost.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toHost.buffer = buffer;
    toHost.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                         0, 0, nullptr, 1, &toHost, 0, nullptr);

    vkEndCommandBuffer(commandBuffer);
}

// Binary PPM (P6): a short text header, then 3 bytes per pixel, top row first.
// The image is RGBA8, so the alpha byte of each pixel is dropped.
void writePpm(const std::string& path, const uint8_t* rgba, VkExtent2D extent) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open '" + path + "' for writing");
    }
    file << "P6\n" << extent.width << " " << extent.height << "\n255\n";
    const size_t pixels = size_t(extent.width) * extent.height;
    std::vector<uint8_t> rgb(pixels * 3);
    for (size_t i = 0; i < pixels; i++) {
        std::memcpy(&rgb[i * 3], &rgba[i * 4], 3);
    }
    file.write(reinterpret_cast<const char*>(rgb.data()), std::streamsize(rgb.size()));
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        const VkExtent2D extent = options.extent;

        // No window, so no window extensions and no surface.
        VkInstance instance = createInstance({});

        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        if (enableValidationLayers) {
            debugMessenger = setupDebugMessenger(instance);
        }

        VkPhysicalDevice physicalDevice = pickPhysicalDevice(instance, VK_NULL_HANDLE);
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        float timestampPeriod = props.limits.timestampPeriod;
        std::cout << "GPU: " << props.deviceName << ", timestampPeriod: " << timestampPeriod << "ns\n";

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice, VK_NULL_HANDLE);
        VkDevice device = createLogicalDevice(physicalDevice, indices.compute.value(), false);

        VkQueue computeQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, indices.compute.value(), 0, &computeQueue);

        StorageImage storageImage = createStorageImage(device, physicalDevice, extent, VK_FORMAT_R8G8B8A8_UNORM);
        StorageImage accumImage = createStorageImage(device, physicalDevice, extent, VK_FORMAT_R32G32B32A32_SFLOAT);

        VkDescriptorSetLayout descriptorSetLayout = createDescriptorSetLayout(device);
        VkDescriptorPool descriptorPool = createDescriptorPool(device);
        VkDescriptorSet descriptorSet = createDescriptorSet(device, descriptorPool, descriptorSetLayout);
        updateDescriptorSet(device, descriptorSet, storageImage.view, accumImage.view);

        VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorSetLayout);
        VkShaderModule shaderModule = createShaderModule(device, "build/raytrace.comp.spv");
        VkPipeline computePipeline = createPipeline(device, shaderModule, pipelineLayout);
        vkDestroyShaderModule(device, shaderModule, nullptr);

        VkCommandPool commandPool = createCommandPool(device, indices.compute.value());
        VkCommandBuffer commandBuffer = createCommandBuffer(device, commandPool);
        VkQueryPool queryPool = createQueryPool(device);
        VkFence fence = createFence(device);

        // Host-visible so the CPU can map it; coherent so no flush or invalidate is needed.
        Buffer readback = createBuffer(device, physicalDevice, VkDeviceSize(extent.width) * extent.height * 4,
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        FrameInputs frameInputs{};
        frameInputs.pipeline = computePipeline;
        frameInputs.pipelineLayout = pipelineLayout;
        frameInputs.descriptorSet = descriptorSet;
        frameInputs.outputImage = storageImage.image;
        frameInputs.accumImage = accumImage.image;
        frameInputs.timestamps = queryPool;
        frameInputs.extent = extent;
        ImageLayouts imageLayouts{};

        GpuTimer timer{};
        std::cout << "Resolution: " << extent.width << "x" << extent.height << "\n";

        // One frame at a time: submit, wait on the fence, read its timestamps. With nothing
        // to present there are no semaphores; the fence is the only CPU-GPU handshake.
        for (uint32_t frameCount = 0;; frameCount++) {
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);
            recordTrace(commandBuffer, frameInputs, frameCount, imageLayouts);
            vkEndCommandBuffer(commandBuffer);

            submitAndWait(device, computeQueue, commandBuffer, fence);

            if (readFrameTime(timer, device, queryPool, timestampPeriod, frameCount)) {
                printReport(timer, props.deviceName, extent);
                break;
            }
        }

        recordReadback(commandBuffer, storageImage.image, extent, readback.buffer);
        submitAndWait(device, computeQueue, commandBuffer, fence);

        void* mapped = nullptr;
        vkMapMemory(device, readback.memory, 0, readback.size, 0, &mapped);
        writePpm(options.outPath, static_cast<const uint8_t*>(mapped), extent);
        vkUnmapMemory(device, readback.memory);
        std::cout << "Image: " << options.outPath << "\n";

        vkDeviceWaitIdle(device);

        destroyBuffer(device, readback);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        destroyStorageImage(device, storageImage);
        destroyStorageImage(device, accumImage);

        vkDestroyQueryPool(device, queryPool, nullptr);
        vkDestroyFence(device, fence, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            destroyDebugMessenger(instance, debugMessenger);
        }

        vkDestroyInstance(instance, nullptr);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
