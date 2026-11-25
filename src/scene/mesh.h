// Scenes on the CPU: the spheres, triangles and materials of a scene, an OBJ loader, a few
// shapes built in code, the built-in scenes, and the packing into the exact byte layout the
// shader reads. Every scene, spheres included, reaches the GPU through storage buffers.
#pragma once

#include "scene/math.h"

#include <cstdint>
#include <string>
#include <vector>

// Material types match LAMBERTIAN and METAL in shaders/raytrace.comp.
enum MaterialType : uint32_t { kLambertian = 0, kMetal = 1 };

struct Material {
    Vec3 albedo;
    uint32_t type = kLambertian;
    float fuzz = 0.0f;
};

struct Triangle {
    Vec3 v0, v1, v2;
    uint32_t material = 0;

    Vec3 centroid() const { return (v0 + v1 + v2) * (1.0f / 3.0f); }
    Aabb bounds() const {
        Aabb b;
        b.grow(v0);
        b.grow(v1);
        b.grow(v2);
        return b;
    }
};

struct Sphere {
    Vec3 center;
    float radius = 0.0f;
    uint32_t material = 0;
};

struct Scene {
    std::vector<Sphere> spheres;
    std::vector<Triangle> triangles;
    std::vector<Material> materials;

    uint32_t addMaterial(Material m) {
        materials.push_back(m);
        return uint32_t(materials.size() - 1);
    }
};

// Shapes built in code, appended to a mesh with the given material.
void appendQuad(Scene& mesh, Vec3 a, Vec3 b, Vec3 c, Vec3 d, uint32_t material);
void appendBox(Scene& mesh, Vec3 min, Vec3 max, uint32_t material);
// A sphere made of triangles: an icosahedron (20 faces) with every face split into 4,
// `subdivisions` times, so it has 20 * 4^subdivisions triangles.
void appendIcosphere(Scene& mesh, Vec3 center, float radius, int subdivisions, uint32_t material);

// Reads the positions (v) and faces (f) of an OBJ file and ignores everything else.
// Faces with more than three corners are split into a fan of triangles.
// Every triangle gets `material`.
void loadObj(Scene& mesh, const std::string& path, uint32_t material);

// Moves and scales the triangles from index `first` on so they fit a box of the given
// height, centred on x and z at `base`, with their lowest point at base.y.
void fitToBox(Scene& mesh, size_t first, Vec3 base, float height);

// The built-in "spheres" scene: the original four spheres (the ground is a sphere of radius 100).
Scene makeSpheresScene();
// The built-in "grid" scene: the ground sphere plus an 8 x 8 grid of small spheres whose colours
// and materials come from pcgHash of their index, exactly as the shader used to compute them.
Scene makeGridScene();

// The built-in "triangles" scene: a ground quad, a smooth metal sphere, a low-poly
// matte sphere and a box, about 400 triangles, posed like the spheres scene.
Scene makeTriangleScene();

// An OBJ model, fitted to stand on the ground where the big metal sphere is, plus the
// ground quad. Matte grey, since OBJ materials are not read.
Scene makeObjScene(const std::string& objPath);

// The GPU layout, std430, matching `Triangle` and `Material` in shaders/raytrace.comp.
// In std430 a vec3 is 16-byte aligned but 12 bytes long, so a 4-byte value fits after it.
struct GpuTriangle {
    float v0[3];
    uint32_t material;
    float v1[3];
    float pad1;
    float v2[3];
    float pad2;
};
static_assert(sizeof(GpuTriangle) == 48, "GpuTriangle must match the shader's std430 layout");

struct GpuMaterial {
    float albedo[3];
    float fuzz;
    uint32_t type;
    uint32_t pad[3];
};
static_assert(sizeof(GpuMaterial) == 32, "GpuMaterial must match the shader's std430 layout");

struct GpuSphere {
    float center[3];
    float radius;
    uint32_t material;
    uint32_t pad[3];
};
static_assert(sizeof(GpuSphere) == 32, "GpuSphere must match the shader's std430 layout");

std::vector<GpuSphere> packSpheres(const std::vector<Sphere>& spheres);
std::vector<GpuTriangle> packTriangles(const std::vector<Triangle>& triangles);
std::vector<GpuMaterial> packMaterials(const std::vector<Material>& materials);
