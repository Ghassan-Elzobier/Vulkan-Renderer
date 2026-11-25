// A bounding volume hierarchy over a scene's triangles, built on the CPU with the binned
// surface area heuristic (SAH), plus a CPU traversal used to prove the tree finds exactly
// the hits brute force finds. The node layout is the one the GPU will read (task 131).
#pragma once

#include "scene/math.h"
#include "scene/mesh.h"

#include <cstdint>
#include <optional>
#include <vector>

// One node, 32 bytes, std430-ready: a vec3 and a uint, twice.
// A leaf has count > 0 and owns triangles [leftOrFirst, leftOrFirst + count).
// An inner node has count == 0; its children are nodes leftOrFirst and leftOrFirst + 1,
// so one index finds both and the node stays small.
struct BvhNode {
    Vec3 boundsMin;
    uint32_t leftOrFirst = 0;
    Vec3 boundsMax;
    uint32_t count = 0;

    bool isLeaf() const { return count > 0; }
};
static_assert(sizeof(BvhNode) == 32, "BvhNode must match the shader's std430 layout");

struct BvhBuildOptions {
    int binCount = 16;      // SAH candidate planes per axis = binCount - 1
    uint32_t maxLeafSize = 8; // a node this small may still split if SAH says it is cheaper
};

struct Bvh {
    std::vector<BvhNode> nodes; // nodes[0] is the root
};

// Builds the tree and reorders `triangles` so every leaf's triangles sit next to each other.
Bvh buildBvh(std::vector<Triangle>& triangles, const BvhBuildOptions& options = {});

// ---- The SAH split: the heart of the build -------------------------------------------------
//
// SAH estimates the cost of tracing a random ray through a node split into L and R as
//     cost = kTraversalCost + (area(L) * count(L) + area(R) * count(R)) / area(parent)
// with one triangle test costing 1. Not splitting (a leaf) costs count(parent).
//
// Binning: along one axis, cut the node's centroid bounds into binCount equal slabs, drop
// every triangle into the slab its centroid falls in (binOf), and keep a count and a box per
// bin. The binCount - 1 planes between bins are the candidates. Sweeping the bins left to
// right and right to left gives every candidate's L and R in O(binCount), not O(count).

constexpr float kTraversalCost = 1.0f;

struct Split {
    int axis = -1;     // -1: no useful split, make a leaf
    int bin = 0;       // triangles with binOf(...) < bin go left, the rest right
    float cost = 0.0f; // SAH cost of this split, comparable with a leaf's cost (count)
};

// Which of binCount bins a centroid falls in along `axis`. The build partitions with this
// same function, so a triangle is on the side the cost was computed for, whatever the rounding.
inline int binOf(Vec3 centroid, int axis, const Aabb& centroidBounds, int binCount) {
    const float lo = centroidBounds.min[axis];
    const float extent = centroidBounds.max[axis] - lo;
    const int bin = int((centroid[axis] - lo) * (float(binCount) / extent));
    return std::clamp(bin, 0, binCount - 1);
}

// Finds the cheapest split of triangles indices[0 .. count) of a node, over all three axes.
// centroids[i] is the centroid of triangles[i]. Lives in sah_split.cpp.
Split findBestSplit(const std::vector<Triangle>& triangles, const std::vector<Vec3>& centroids,
                    const uint32_t* indices, uint32_t count, const Aabb& centroidBounds, int binCount);

// ---- Checking and measuring ------------------------------------------------------------------

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

struct Hit {
    float t;
    uint32_t triangle; // index into the (reordered) triangles
};

// Möller-Trumbore, the same test and the same 0.001 minimum t as the shader.
std::optional<float> intersectTriangle(const Ray& ray, const Triangle& tri);
std::optional<Hit> intersectBruteForce(const std::vector<Triangle>& triangles, const Ray& ray);
std::optional<Hit> intersectBvh(const Bvh& bvh, const std::vector<Triangle>& triangles, const Ray& ray);

struct BvhStats {
    uint32_t nodes = 0;
    uint32_t leaves = 0;
    uint32_t maxDepth = 0;
    uint32_t maxLeafSize = 0;
    float averageLeafSize = 0.0f;
    float sahCost = 0.0f; // the whole tree's SAH cost; lower means fewer tests per ray
};
BvhStats measureBvh(const Bvh& bvh);
