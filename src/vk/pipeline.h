// The compute kernel and its inputs: the descriptor set that points the shader at its images and buffers,
// the pipeline layout (descriptors plus a 4-byte push constant), and the compiled shader.
#pragma once

#include "vk/vulkan.h"

#include <string>

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device);
VkDescriptorPool createDescriptorPool(VkDevice device);
VkDescriptorSet createDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);
// Binding 0 is the output image, 1 the accumulation image, 2 the triangles, 3 the materials,
// 4 the spheres.
void updateDescriptorSet(VkDevice device, VkDescriptorSet set, VkImageView outputView, VkImageView accumView,
                         VkBuffer triangles, VkBuffer materials, VkBuffer spheres);

VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
VkShaderModule createShaderModule(VkDevice device, const std::string& filename);
// scene sets the shader's SCENE specialization constant (constant_id 0), see raytrace.comp.
VkPipeline createPipeline(VkDevice device, VkShaderModule shaderModule, VkPipelineLayout pipelineLayout,
                          uint32_t scene = 0);
