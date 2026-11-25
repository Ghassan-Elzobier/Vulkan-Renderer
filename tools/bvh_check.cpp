// Checks the CPU BVH: for each test mesh, builds the tree, fires random rays and compares the
// BVH's closest hit with brute force's, then prints the tree's statistics and the CPU speed-up.
// Exits 0 when every ray agrees, 1 when any differ, 2 on an error.
//
// Usage: Bvh_Check [--obj file.obj] [--rays N] [--bins N]

#include "scene/bvh.h"
#include "scene/mesh.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double millisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// A small fixed-seed generator, so every run fires the same rays.
struct Rng {
    uint32_t state;
    float next() {
        state = state * 747796405u + 2891336453u;
        uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        return float((word >> 22u) ^ word) / 4294967296.0f;
    }
    float range(float lo, float hi) { return lo + (hi - lo) * next(); }
};

// Rays start anywhere in the scene's box (a bit larger) and point anywhere, so they cross the
// tree at every depth and from every side, not only from the camera.
std::vector<Ray> makeRays(const std::vector<Triangle>& triangles, uint32_t count) {
    Aabb bounds;
    for (const Triangle& t : triangles) bounds.grow(t.bounds());
    const Vec3 pad = bounds.extent() * 0.1f;
    Rng rng{12345u};
    std::vector<Ray> rays;
    rays.reserve(count);
    while (rays.size() < count) {
        Vec3 d{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)};
        if (dot(d, d) < 1e-4f || dot(d, d) > 1.0f) continue;
        Vec3 o{rng.range(bounds.min.x - pad.x, bounds.max.x + pad.x),
               rng.range(bounds.min.y - pad.y, bounds.max.y + pad.y),
               rng.range(bounds.min.z - pad.z, bounds.max.z + pad.z)};
        rays.push_back({o, normalize(d)});
    }
    return rays;
}

// Returns the number of rays where the BVH and brute force disagree.
uint32_t check(const std::string& name, std::vector<Triangle> triangles, uint32_t rayCount, int bins) {
    const std::vector<Ray> rays = makeRays(triangles, rayCount);

    const auto buildStart = Clock::now();
    BvhBuildOptions options;
    options.binCount = bins;
    const Bvh bvh = buildBvh(triangles, options); // reorders triangles, brute force uses the same order
    const double buildMs = millisecondsSince(buildStart);
    const BvhStats stats = measureBvh(bvh);

    std::vector<std::optional<Hit>> expected(rays.size());
    const auto bruteStart = Clock::now();
    for (size_t i = 0; i < rays.size(); i++) expected[i] = intersectBruteForce(triangles, rays[i]);
    const double bruteMs = millisecondsSince(bruteStart);

    std::vector<std::optional<Hit>> actual(rays.size());
    const auto bvhStart = Clock::now();
    for (size_t i = 0; i < rays.size(); i++) actual[i] = intersectBvh(bvh, triangles, rays[i]);
    const double bvhMs = millisecondsSince(bvhStart);

    // Both use the same triangle test, so the closest t must match exactly. Two triangles can
    // share an edge and give the same t, so compare t, not which triangle won.
    uint32_t mismatches = 0, hits = 0;
    for (size_t i = 0; i < rays.size(); i++) {
        const bool same = expected[i].has_value() == actual[i].has_value() &&
                          (!expected[i] || expected[i]->t == actual[i]->t);
        if (expected[i]) hits++;
        if (!same) {
            if (mismatches < 5) {
                std::printf("  MISMATCH ray %zu: brute force t=%g, BVH t=%g\n", i,
                            expected[i] ? expected[i]->t : -1.0, actual[i] ? actual[i]->t : -1.0);
            }
            mismatches++;
        }
    }

    std::printf("%s: %zu triangles, %u nodes, %u leaves (avg %.2f, max %u triangles), depth %u\n",
                name.c_str(), triangles.size(), stats.nodes, stats.leaves, stats.averageLeafSize,
                stats.maxLeafSize, stats.maxDepth);
    std::printf("  SAH cost %.2f (brute force would be %zu), build %.2f ms\n", stats.sahCost,
                triangles.size(), buildMs);
    std::printf("  %u rays, %u hit: brute force %.1f ms, BVH %.1f ms (%.1fx), %u mismatches\n",
                rayCount, hits, bruteMs, bvhMs, bruteMs / bvhMs, mismatches);
    return mismatches;
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string objPath;
        uint32_t rayCount = 20000;
        int bins = BvhBuildOptions{}.binCount;
        for (int i = 1; i < argc; i++) {
            const std::string arg = argv[i];
            if (arg == "--obj" && i + 1 < argc) {
                objPath = argv[++i];
            } else if (arg == "--rays" && i + 1 < argc) {
                rayCount = uint32_t(std::strtoul(argv[++i], nullptr, 10));
            } else if (arg == "--bins" && i + 1 < argc) {
                bins = std::max(2, std::atoi(argv[++i]));
            } else {
                std::fprintf(stderr, "Unknown argument '%s'\n", arg.c_str());
                return 2;
            }
        }

        uint32_t mismatches = 0;
        mismatches += check("triangles scene", makeTriangleScene().triangles, rayCount, bins);

        Scene dense;
        appendIcosphere(dense, {0, 0, 0}, 1.0f, 5, 0); // 20480 triangles, all the same size
        mismatches += check("icosphere", dense.triangles, rayCount, bins);

        // Many separate shapes of very different sizes: where a good split matters most.
        Scene clutter;
        Rng rng{777u};
        for (int i = 0; i < 200; i++) {
            const Vec3 c{rng.range(-10, 10), rng.range(-10, 10), rng.range(-10, 10)};
            const float r = 0.05f + 0.6f * rng.next() * rng.next();
            if (i % 3 == 0) {
                appendBox(clutter, c - Vec3{r, r, r}, c + Vec3{r, r, r}, 0);
            } else {
                appendIcosphere(clutter, c, r, i % 2 + 1, 0); // 80 or 320 triangles
            }
        }
        mismatches += check("clutter", clutter.triangles, rayCount, bins);

        if (!objPath.empty()) {
            Scene obj;
            loadObj(obj, objPath, 0);
            mismatches += check(objPath, obj.triangles, rayCount, bins);
        }

        std::printf(mismatches == 0 ? "PASS\n" : "FAIL: %u rays disagree\n", mismatches);
        return mismatches == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 2;
    }
}
