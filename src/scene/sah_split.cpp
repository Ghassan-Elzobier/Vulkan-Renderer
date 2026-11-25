#include "scene/bvh.h"

// YOUR TURN (task 130): replace this placeholder with the binned SAH sweep described in bvh.h.
//
// The placeholder splits the longest axis at its middle bin and works out what that one split
// costs. It builds a valid tree, so Bvh_Check passes, but it never looks at the other
// binCount - 2 planes or the other two axes. A real binned SAH should cut the SAH cost
// Bvh_Check prints. Keep the signature and the Split it returns; buildBvh relies on both.
Split findBestSplit(const std::vector<Triangle>& triangles, const std::vector<Vec3>& centroids,
                    const uint32_t* indices, uint32_t count, const Aabb& centroidBounds, int binCount) {
    const Vec3 e = centroidBounds.extent();
    const int axis = (e.x >= e.y && e.x >= e.z) ? 0 : (e.y >= e.z ? 1 : 2);
    if (e[axis] <= 0.0f) return {}; // every centroid in one spot: no plane separates them

    const int bin = binCount / 2;
    Aabb left, right;
    uint32_t leftCount = 0, rightCount = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t tri = indices[i];
        if (binOf(centroids[tri], axis, centroidBounds, binCount) < bin) {
            left.grow(triangles[tri].bounds());
            leftCount++;
        } else {
            right.grow(triangles[tri].bounds());
            rightCount++;
        }
    }

    Aabb parent = left;
    parent.grow(right);
    const float cost =
        kTraversalCost + (left.area() * float(leftCount) + right.area() * float(rightCount)) / parent.area();
    return {axis, bin, cost};
}
