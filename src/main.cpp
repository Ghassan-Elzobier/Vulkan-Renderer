#include "vk/vulkan.h"
#include "vk/context.h"
#include "vk/swapchain.h"
#include "vk/sync.h"
#include "vk/resources.h"
#include "vk/pipeline.h"
#include "render/frame.h"
#include "bench/gpu_timer.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

// main() only wires the pieces together: build everything, run the frame loop, clean up in reverse.

int main() {
    try {
        if (!glfwInit()) {
            std::cerr << "Installation failed\n";
            return 1;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Ray Tracer", nullptr, nullptr);
        if (window == nullptr) {
            std::cerr << "Failed to create window\n";
            glfwTerminate();
            return 1;
        }

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        VkInstance instance = createInstance({glfwExtensions, glfwExtensions + glfwExtensionCount});

        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        if (enableValidationLayers) {
            debugMessenger = setupDebugMessenger(instance);
        }

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }

        VkPhysicalDevice physicalDevice = pickPhysicalDevice(instance, surface);
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        float timestampPeriod = props.limits.timestampPeriod;
        std::cout << "GPU: " << props.deviceName << ", timestampPeriod: " << timestampPeriod << "ns\n";

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
        VkDevice device = createLogicalDevice(physicalDevice, indices.compute.value(), true);

        VkQueue computeQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, indices.compute.value(), 0, &computeQueue);
        VkQueue presentQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, indices.present.value(), 0, &presentQueue);

        Swapchain swapchain = createSwapchain(device, physicalDevice, surface, window, indices);

        StorageImage storageImage = createStorageImage(device, physicalDevice, swapchain.extent, VK_FORMAT_R8G8B8A8_UNORM);
        StorageImage accumImage = createStorageImage(device, physicalDevice, swapchain.extent, VK_FORMAT_R32G32B32A32_SFLOAT);

        VkDescriptorSetLayout descriptorSetLayout = createDescriptorSetLayout(device);
        VkDescriptorPool descriptorPool = createDescriptorPool(device);
        VkDescriptorSet descriptorSet = createDescriptorSet(device, descriptorPool, descriptorSetLayout);
        updateDescriptorSet(device, descriptorSet, storageImage.view, accumImage.view);

        VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorSetLayout);
        VkShaderModule shaderModule = createShaderModule(device, "build/raytrace.comp.spv");
        VkPipeline computePipeline = createPipeline(device, shaderModule, pipelineLayout);
        vkDestroyShaderModule(device, shaderModule, nullptr);

        VkCommandPool commandPool = createCommandPool(device, indices.compute.value());
        VkCommandBuffer commandBuffer =  createCommandBuffer(device, commandPool);
        VkQueryPool queryPool = createQueryPool(device);

        VkSemaphore imageAvailable = createSemaphore(device);
        std::vector<VkSemaphore> renderFinished(swapchain.images.size());
        for (size_t i = 0; i < renderFinished.size(); i++) {
            renderFinished[i] = createSemaphore(device);
        }
        VkFence inFlight = createFence(device);

        FrameInputs frameInputs{};
        frameInputs.pipeline = computePipeline;
        frameInputs.pipelineLayout = pipelineLayout;
        frameInputs.descriptorSet = descriptorSet;
        frameInputs.outputImage = storageImage.image;
        frameInputs.accumImage = accumImage.image;
        frameInputs.timestamps = queryPool;
        frameInputs.extent = swapchain.extent;
        ImageLayouts imageLayouts{};

        uint32_t frameCount = 0;
        GpuTimer timer{};

        const uint32_t width = swapchain.extent.width;
        const uint32_t height = swapchain.extent.height;
        std::cout << "Resolution: " << width << "x" << height << "\n";
        
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            vkWaitForFences(device, 1, &inFlight, VK_TRUE, UINT64_MAX);
            if (frameCount != 0) {
                if (readFrameTime(timer, device, queryPool, timestampPeriod, frameCount - 1)) {
                    printReport(timer, props.deviceName, swapchain.extent);
                    break;
                }
            }
            vkResetFences(device, 1, &inFlight);
            uint32_t imageIndex;
            vkAcquireNextImageKHR(device, swapchain.handle, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);

            recordFrame(commandBuffer, frameInputs, frameCount, swapchain.images[imageIndex], imageLayouts);

            VkSemaphore waitSemaphores[] = { imageAvailable };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
            VkSemaphore signalSemaphores[] = { renderFinished[imageIndex] };


            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;
            vkQueueSubmit(computeQueue, 1, &submitInfo, inFlight);

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain.handle;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(presentQueue, &presentInfo);
            frameCount++;
        }

        vkDeviceWaitIdle(device);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        destroyStorageImage(device, storageImage);
        destroyStorageImage(device, accumImage);

        destroySwapchain(device, swapchain);
        vkDestroySemaphore(device, imageAvailable, nullptr);
        for (VkSemaphore semaphore : renderFinished) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
        vkDestroyQueryPool(device, queryPool, nullptr);
        vkDestroyFence(device, inFlight, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);

        if (enableValidationLayers) {
            destroyDebugMessenger(instance, debugMessenger);
        }

        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

