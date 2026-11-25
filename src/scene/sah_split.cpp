#include "scene/bvh.h"

#include <limits>
#include <vector>

// The binned SAH split, as described in bvh.h. For each axis: drop the triangles into bins by
// centroid, sweep the bins from both ends to get every wall's left and right side, score each
// wall with the SAH formula and keep the cheapest wall over all three axes.
//
// Measured with Bvh_Check (16 bins), against the old midpoint placeholder: icosphere SAH cost
// 40.18 -> 37.35, clutter 14.51 -> 12.87. The built-in triangles scene gets worse, 4.98 -> 5.74:
// its two ground triangles have centroids 13 units out, so the bins are 1.7 units wide and every
// object lands in one or two bins near the middle. 256 bins gives 4.80. Real models have no such
// giant triangles, so 16 bins stays the default.
Split findBestSplit(const std::vector<Triangle>& triangles, const std::vector<Vec3>& centroids,
                    const uint32_t* indices, uint32_t count, const Aabb& centroidBounds, int binCount) {
    Split best; // axis = -1 means "no split found", so the caller makes a leaf
    best.cost = std::numeric_limits<float>::infinity();

    struct Bin {
        Aabb box;
        uint32_t count = 0;
    };
    std::vector<Bin> bins(binCount);
    const int walls = binCount - 1; // wall w sits between bin w and bin w + 1
    std::vector<float> leftArea(walls), rightArea(walls);
    std::vector<uint32_t> leftCount(walls), rightCount(walls);

    for (int axis = 0; axis < 3; axis++) {
        if (centroidBounds.extent()[axis] <= 0.0f) continue; // all centres level: no wall separates them

        // 1. Drop every triangle into the bin its centroid falls in.
        for (Bin& bin : bins) bin = Bin{};
        for (uint32_t i = 0; i < count; i++) {
            const uint32_t tri = indices[i];
            Bin& bin = bins[binOf(centroids[tri], axis, centroidBounds, binCount)];
            bin.count++;
            bin.box.grow(triangles[tri].bounds());
        }

        // 2. Running totals from both ends. After step w, the left total holds bins 0..w (the left
        // side of wall w) and the right total holds bins (binCount-1-w)..binCount-1 (the right side
        // of wall binCount-2-w). One pass fills both tables.
        Aabb leftBox, rightBox;
        uint32_t leftSum = 0, rightSum = 0;
        for (int w = 0; w < walls; w++) {
            leftSum += bins[w].count;
            leftBox.grow(bins[w].box);
            leftCount[w] = leftSum;
            leftArea[w] = leftBox.area();

            const int r = binCount - 1 - w;
            rightSum += bins[r].count;
            rightBox.grow(bins[r].box);
            rightCount[r - 1] = rightSum;
            rightArea[r - 1] = rightBox.area();
        }

        // The parent is every bin together: the left side of the last wall plus the last bin.
        Aabb parent = leftBox;
        parent.grow(bins[binCount - 1].box);
        const float parentArea = parent.area();
        if (parentArea <= 0.0f) continue; // a flat sheet of zero size: area tells us nothing

        // 3. Score every wall. A wall with an empty side is not a split, so skip it.
        for (int w = 0; w < walls; w++) {
            if (leftCount[w] == 0 || rightCount[w] == 0) continue;
            const float cost = kTraversalCost +
                (leftArea[w] * float(leftCount[w]) + rightArea[w] * float(rightCount[w])) / parentArea;
            if (cost < best.cost) best = Split{axis, w + 1, cost};
        }
    }
    return best;
}
