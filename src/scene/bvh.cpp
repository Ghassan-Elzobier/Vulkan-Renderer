#include "scene/bvh.h"

#include <utility>

namespace {

struct Builder {
    std::vector<Triangle>& triangles;
    std::vector<Vec3> centroids;
    std::vector<uint32_t> indices; // the build shuffles these, not the triangles
    BvhBuildOptions options;
    Bvh bvh;

    // Fits node's box around its triangles and returns the box around their centroids.
    Aabb fitBounds(BvhNode& node) {
        Aabb bounds, centroidBounds;
        for (uint32_t i = 0; i < node.count; i++) {
            const uint32_t tri = indices[node.leftOrFirst + i];
            bounds.grow(triangles[tri].bounds());
            centroidBounds.grow(centroids[tri]);
        }
        node.boundsMin = bounds.min;
        node.boundsMax = bounds.max;
        return centroidBounds;
    }

    void subdivide(uint32_t nodeIndex) {
        BvhNode& node = bvh.nodes[nodeIndex];
        const Aabb centroidBounds = fitBounds(node);
        if (node.count <= 1) return;

        const Split split = findBestSplit(triangles, centroids, &indices[node.leftOrFirst], node.count,
                                          centroidBounds, options.binCount);
        // A leaf costs one test per triangle. Split only when that is dearer than the split,
        // or the leaf would be too big for the GPU's fixed-size loop.
        const float leafCost = float(node.count);
        const bool mustSplit = node.count > options.maxLeafSize;
        if (split.axis < 0 || (split.cost >= leafCost && !mustSplit)) return;

        // Partition, like quicksort's: left-side triangles to the front, the rest to the back.
        uint32_t i = node.leftOrFirst;
        uint32_t j = node.leftOrFirst + node.count;
        while (i < j) {
            if (binOf(centroids[indices[i]], split.axis, centroidBounds, options.binCount) < split.bin) {
                i++;
            } else {
                std::swap(indices[i], indices[--j]);
            }
        }
        const uint32_t leftCount = i - node.leftOrFirst;
        if (leftCount == 0 || leftCount == node.count) return; // all on one side: stay a leaf

        // Children are pushed as a pair, so the right child is always left + 1.
        const uint32_t left = uint32_t(bvh.nodes.size());
        BvhNode leftNode, rightNode;
        leftNode.leftOrFirst = node.leftOrFirst;
        leftNode.count = leftCount;
        rightNode.leftOrFirst = i;
        rightNode.count = node.count - leftCount;
        bvh.nodes.push_back(leftNode); // may reallocate: `node` is dangling from here on
        bvh.nodes.push_back(rightNode);
        bvh.nodes[nodeIndex].leftOrFirst = left;
        bvh.nodes[nodeIndex].count = 0;

        subdivide(left);
        subdivide(left + 1);
    }
};

// Slab test: the ray is inside the box between the latest "enter" and the earliest "exit"
// over the three axes. Returns the entry distance, or +inf for a miss.
float intersectAabb(const Ray& ray, Vec3 invDir, Vec3 bmin, Vec3 bmax, float maxT) {
    float tmin = 0.0f, tmax = maxT;
    for (int a = 0; a < 3; a++) {
        float t0 = (bmin[a] - ray.origin[a]) * invDir[a];
        float t1 = (bmax[a] - ray.origin[a]) * invDir[a];
        if (t0 > t1) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
    }
    return tmin <= tmax ? tmin : std::numeric_limits<float>::infinity();
}

} // namespace

Bvh buildBvh(std::vector<Triangle>& triangles, const BvhBuildOptions& options) {
    Builder b{triangles, {}, {}, options, {}};
    b.centroids.reserve(triangles.size());
    b.indices.reserve(triangles.size());
    for (uint32_t i = 0; i < triangles.size(); i++) {
        b.centroids.push_back(triangles[i].centroid());
        b.indices.push_back(i);
    }
    // A binary tree with N leaves has 2N - 1 nodes, and there are at most N leaves.
    b.bvh.nodes.reserve(triangles.empty() ? 1 : 2 * triangles.size() - 1);

    BvhNode root;
    root.leftOrFirst = 0;
    root.count = uint32_t(triangles.size());
    b.bvh.nodes.push_back(root);
    if (!triangles.empty()) b.subdivide(0);

    // Put the triangles in leaf order, so a leaf reads one contiguous run of memory.
    std::vector<Triangle> ordered;
    ordered.reserve(triangles.size());
    for (uint32_t index : b.indices) ordered.push_back(triangles[index]);
    triangles = std::move(ordered);
    return std::move(b.bvh);
}

std::optional<float> intersectTriangle(const Ray& ray, const Triangle& tri) {
    const Vec3 e1 = tri.v1 - tri.v0;
    const Vec3 e2 = tri.v2 - tri.v0;
    const Vec3 p = cross(ray.direction, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-8f) return std::nullopt; // ray parallel to the triangle
    const float invDet = 1.0f / det;
    const Vec3 s = ray.origin - tri.v0;
    const float u = dot(s, p) * invDet;
    if (u < 0.0f || u > 1.0f) return std::nullopt;
    const Vec3 q = cross(s, e1);
    const float v = dot(ray.direction, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return std::nullopt;
    const float t = dot(e2, q) * invDet;
    if (t <= 0.001f) return std::nullopt;
    return t;
}

std::optional<Hit> intersectBruteForce(const std::vector<Triangle>& triangles, const Ray& ray) {
    std::optional<Hit> best;
    for (uint32_t i = 0; i < triangles.size(); i++) {
        auto t = intersectTriangle(ray, triangles[i]);
        if (t && (!best || *t < best->t)) best = Hit{*t, i};
    }
    return best;
}

std::optional<Hit> intersectBvh(const Bvh& bvh, const std::vector<Triangle>& triangles, const Ray& ray) {
    const Vec3 invDir{1.0f / ray.direction.x, 1.0f / ray.direction.y, 1.0f / ray.direction.z};
    std::optional<Hit> best;
    float bestT = std::numeric_limits<float>::infinity();

    // A fixed stack, as the shader will have. 64 is far deeper than a SAH tree gets.
    uint32_t stack[64];
    uint32_t top = 0;
    if (intersectAabb(ray, invDir, bvh.nodes[0].boundsMin, bvh.nodes[0].boundsMax, bestT) < bestT) {
        stack[top++] = 0;
    }

    while (top > 0) {
        const BvhNode& node = bvh.nodes[stack[--top]];
        if (node.isLeaf()) {
            for (uint32_t i = node.leftOrFirst; i < node.leftOrFirst + node.count; i++) {
                auto t = intersectTriangle(ray, triangles[i]);
                if (t && *t < bestT) {
                    bestT = *t;
                    best = Hit{*t, i};
                }
            }
            continue;
        }
        // Visit the nearer child first, so bestT shrinks early and prunes the farther one.
        uint32_t nearChild = node.leftOrFirst, farChild = node.leftOrFirst + 1;
        float tNear = intersectAabb(ray, invDir, bvh.nodes[nearChild].boundsMin, bvh.nodes[nearChild].boundsMax, bestT);
        float tFar = intersectAabb(ray, invDir, bvh.nodes[farChild].boundsMin, bvh.nodes[farChild].boundsMax, bestT);
        if (tFar < tNear) {
            std::swap(nearChild, farChild);
            std::swap(tNear, tFar);
        }
        // Push the far child first so the near one is popped next.
        if (tFar < bestT) stack[top++] = farChild;
        if (tNear < bestT) stack[top++] = nearChild;
    }
    return best;
}

BvhStats measureBvh(const Bvh& bvh) {
    BvhStats stats;
    stats.nodes = uint32_t(bvh.nodes.size());
    const auto area = [](const BvhNode& n) { return Aabb{n.boundsMin, n.boundsMax}.area(); };
    const float rootArea = area(bvh.nodes[0]);

    struct Item { uint32_t node, depth; };
    std::vector<Item> todo{{0, 1}};
    uint32_t leafTriangles = 0;
    while (!todo.empty()) {
        const Item item = todo.back();
        todo.pop_back();
        const BvhNode& n = bvh.nodes[item.node];
        stats.maxDepth = std::max(stats.maxDepth, item.depth);
        // Each node's cost is weighted by the chance a ray that hits the root also hits it.
        const float p = rootArea > 0.0f ? area(n) / rootArea : 1.0f;
        if (n.isLeaf()) {
            stats.leaves++;
            leafTriangles += n.count;
            stats.maxLeafSize = std::max(stats.maxLeafSize, n.count);
            stats.sahCost += p * float(n.count);
        } else {
            stats.sahCost += p * kTraversalCost;
            todo.push_back({n.leftOrFirst, item.depth + 1});
            todo.push_back({n.leftOrFirst + 1, item.depth + 1});
        }
    }
    stats.averageLeafSize = stats.leaves ? float(leafTriangles) / float(stats.leaves) : 0.0f;
    return stats;
}
