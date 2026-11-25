// The scene's GPU copy: its triangles, materials and spheres in three device-local storage
// buffers, bound at descriptor bindings 2, 3 and 4.
#pragma once

#include "vk/vulkan.h"
#include "vk/resources.h"
#include "scene/mesh.h"

struct SceneBuffers {
    Buffer triangles;
    Buffer materials;
    Buffer spheres;
};

// Packs the scene into the shader's layout and uploads it. An empty list still gets one
// zeroed entry, because a descriptor must point at a real buffer even when
// the chosen scene never reads it.
SceneBuffers uploadScene(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
                         VkCommandPool commandPool, const Scene& scene);
void destroySceneBuffers(VkDevice device, const SceneBuffers& scene);
