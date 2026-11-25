#include "render/scene_buffers.h"

#include <vector>

SceneBuffers uploadScene(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
                         VkCommandPool commandPool, const Scene& scene) {
    std::vector<GpuTriangle> triangles = packTriangles(scene.triangles);
    std::vector<GpuMaterial> materials = packMaterials(scene.materials);
    std::vector<GpuSphere> spheres = packSpheres(scene.spheres);
    if (triangles.empty()) triangles.push_back({});
    if (materials.empty()) materials.push_back({});
    if (spheres.empty()) spheres.push_back({});

    auto upload = [&](const void* data, size_t bytes) {
        return createFilledBuffer(device, physicalDevice, queue, commandPool, data, bytes,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    };
    SceneBuffers buffers{};
    buffers.triangles = upload(triangles.data(), sizeof(GpuTriangle) * triangles.size());
    buffers.materials = upload(materials.data(), sizeof(GpuMaterial) * materials.size());
    buffers.spheres = upload(spheres.data(), sizeof(GpuSphere) * spheres.size());
    return buffers;
}

void destroySceneBuffers(VkDevice device, const SceneBuffers& scene) {
    destroyBuffer(device, scene.triangles);
    destroyBuffer(device, scene.materials);
    destroyBuffer(device, scene.spheres);
}
