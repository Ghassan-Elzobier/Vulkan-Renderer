// The compute kernel and its inputs: the descriptor set that points the shader at its images,
// the pipeline layout (descriptors plus an 8-byte push constant), and the compiled shader.
#pragma once

#include "vk/vulkan.h"

#include <string>

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device);
VkDescriptorPool createDescriptorPool(VkDevice device);
VkDescriptorSet createDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);
// Binding 0 is the output image, binding 1 the accumulation image.
void updateDescriptorSet(VkDevice device, VkDescriptorSet set, VkImageView outputView, VkImageView accumView);

VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
VkShaderModule createShaderModule(VkDevice device, const std::string& filename);
VkPipeline createPipeline(VkDevice device, VkShaderModule shaderModule, VkPipelineLayout pipelineLayout);
