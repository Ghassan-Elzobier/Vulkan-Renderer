#include "render/scene_buffers.h"

#include <vector>

SceneBuffers uploadScene(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
                         VkCommandPool commandPool, const Mesh& mesh) {
    std::vector<GpuTriangle> triangles = packTriangles(mesh.triangles);
    std::vector<GpuMaterial> materials = packMaterials(mesh.materials);
    if (triangles.empty()) triangles.push_back({});
    if (materials.empty()) materials.push_back({});

    SceneBuffers scene{};
    scene.triangles = createFilledBuffer(device, physicalDevice, queue, commandPool, triangles.data(),
                                         sizeof(GpuTriangle) * triangles.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    scene.materials = createFilledBuffer(device, physicalDevice, queue, commandPool, materials.data(),
                                         sizeof(GpuMaterial) * materials.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    return scene;
}

void destroySceneBuffers(VkDevice device, const SceneBuffers& scene) {
    destroyBuffer(device, scene.triangles);
    destroyBuffer(device, scene.materials);
}
