// Records one frame into a command buffer: barriers, the dispatch between two timestamps,
// then (in the window) the copy from the output image into this frame's swapchain image.
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
    uint32_t scene = 0; // which scene the shader traces, see SCENE_* in raytrace.comp
};

// The layouts the two storage images were left in by the previous frame.
// Both start UNDEFINED, and recordFrame updates them.
struct ImageLayouts {
    VkImageLayout output = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout accum = VK_IMAGE_LAYOUT_UNDEFINED;
};

// The shader's push constants, in the same order as its PushConstants block.
struct PushConstants {
    uint32_t frameCount;
    uint32_t scene;
};

// Records the barriers and the timed dispatch, with no begin or end, so both the window
// and the windowless harness trace a frame the same way. Leaves both images in GENERAL.
void recordTrace(VkCommandBuffer commandBuffer, const FrameInputs& in, uint32_t frameCount,
                 ImageLayouts& layouts);

// recordTrace, then the copy into the swapchain image, in one begun-and-ended command buffer.
// swapchainImage is the image acquired for this frame, which can be a different one every time.
void recordFrame(VkCommandBuffer commandBuffer, const FrameInputs& in, uint32_t frameCount,
                 VkImage swapchainImage, ImageLayouts& layouts);
