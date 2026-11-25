// The scene's GPU copy: a mesh's triangles and materials in two device-local storage buffers,
// bound at descriptor bindings 2 and 3.
#pragma once

#include "vk/vulkan.h"
#include "vk/resources.h"
#include "scene/mesh.h"

struct SceneBuffers {
    Buffer triangles;
    Buffer materials;
};

// Packs the mesh into the shader's layout and uploads it. An empty mesh still gets one
// zeroed triangle and material, because a descriptor must point at a real buffer even when
// the chosen scene never reads it.
SceneBuffers uploadScene(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
                         VkCommandPool commandPool, const Mesh& mesh);
void destroySceneBuffers(VkDevice device, const SceneBuffers& scene);
