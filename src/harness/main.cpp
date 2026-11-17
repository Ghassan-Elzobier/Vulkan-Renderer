#include "vk/vulkan.h"
#include "vk/context.h"
#include "vk/sync.h"
#include "vk/resources.h"
#include "vk/pipeline.h"
#include "render/frame.h"
#include "bench/gpu_timer.h"
#include "image/image_io.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// The windowless compute harness: the same path tracer and the same timing as the window,
// with no surface, swapchain or present semaphores. It runs the warm-up and measured frames and
// prints the benchmark report. Then it renders the image again from frame 0 for exactly --spp
// samples per pixel, so the saved image never depends on the timer's frame counts, and saves it.
//
// Usage: Ray_Tracing_Harness [--scene spheres|grid] [--spp N] [--out file.png|file.ppm]
//                            [--size WIDTHxHEIGHT] [--skip-timing]

namespace {

// The scene numbers match SCENE_* in shaders/raytrace.comp.
const char* const kSceneNames[] = {"spheres", "grid"};

struct Options {
    std::string outPath = "harness.png";
    VkExtent2D extent{800, 600}; // the window's size, so the numbers compare like with like
    uint32_t scene = 0;
    uint32_t samplesPerPixel = 1024;
    bool skipTiming = false;
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
        } else if (arg == "--scene" && i + 1 < argc) {
            const std::string name = argv[++i];
            bool found = false;
            for (uint32_t s = 0; s < std::size(kSceneNames); s++) {
                if (name == kSceneNames[s]) {
                    options.scene = s;
                    found = true;
                }
            }
            if (!found) {
                throw std::runtime_error("--scene wants spheres or grid, got '" + name + "'");
            }
        } else if (arg == "--spp" && i + 1 < argc) {
            const long spp = std::strtol(argv[++i], nullptr, 10);
            if (spp <= 0) {
                throw std::runtime_error("--spp wants a positive number, got '" + std::string(argv[i]) + "'");
            }
            options.samplesPerPixel = uint32_t(spp);
        } else if (arg == "--skip-timing") {
            options.skipTiming = true;
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
        frameInputs.scene = options.scene;
        ImageLayouts imageLayouts{};

        std::cout << "Resolution: " << extent.width << "x" << extent.height
                  << ", scene: " << kSceneNames[options.scene] << "\n";

        // Records, submits and waits for one frame. With nothing to present there are no
        // semaphores; the fence is the only CPU-GPU handshake.
        auto traceFrame = [&](uint32_t frameCount) {
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);
            recordTrace(commandBuffer, frameInputs, frameCount, imageLayouts);
            vkEndCommandBuffer(commandBuffer);
            submitAndWait(device, computeQueue, commandBuffer, fence);
        };

        // Phase 1, the benchmark: warm-up and measured frames, timed one at a time.
        if (!options.skipTiming) {
            GpuTimer timer{};
            for (uint32_t frameCount = 0;; frameCount++) {
                traceFrame(frameCount);
                if (readFrameTime(timer, device, queryPool, timestampPeriod, frameCount)) {
                    printReport(timer, props.deviceName, extent);
                    break;
                }
            }
        }

        // Phase 2, the image: start again from frame 0, which makes the shader overwrite the
        // accumulated sum instead of adding to it, and stop after exactly samplesPerPixel frames.
        // Frame i always uses the same random seeds, so the same GPU gives the same bytes.
        for (uint32_t frameCount = 0; frameCount < options.samplesPerPixel; frameCount++) {
            traceFrame(frameCount);
        }

        recordReadback(commandBuffer, storageImage.image, extent, readback.buffer);
        submitAndWait(device, computeQueue, commandBuffer, fence);

        void* mapped = nullptr;
        vkMapMemory(device, readback.memory, 0, readback.size, 0, &mapped);
        const Image image = fromRgba(static_cast<const uint8_t*>(mapped), extent.width, extent.height);
        vkUnmapMemory(device, readback.memory);
        saveImage(options.outPath, image);
        std::cout << "Image: " << options.outPath << " (" << options.samplesPerPixel << " samples per pixel)\n";

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
