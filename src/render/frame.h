// Records one frame into a command buffer: barriers, the dispatch between two timestamps,
// then the copy from the output image into this frame's swapchain image.
// It takes plain handles only, so it does not depend on how resources or the swapchain are built.
#pragma once

#include "vk/vulkan.h"

#include <cstdint>

// What stays the same from frame to frame.
struct FrameInputs {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet;
    VkImage outputImage;
    VkImage accumImage;
    VkQueryPool timestamps;
    VkExtent2D extent;
};

// The layouts the two storage images were left in by the previous frame.
// Both start UNDEFINED, and recordFrame updates them.
struct ImageLayouts {
    VkImageLayout output = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout accum = VK_IMAGE_LAYOUT_UNDEFINED;
};

// swapchainImage is the image acquired for this frame, which can be a different one every time.
void recordFrame(VkCommandBuffer commandBuffer, const FrameInputs& in, uint32_t frameCount,
                 VkImage swapchainImage, ImageLayouts& layouts);
