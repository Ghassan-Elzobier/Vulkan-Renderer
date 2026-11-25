// The little vector maths the CPU side of the scene needs: a 3-float vector and an axis-aligned
// box. Plain C++ with no Vulkan or GLM, so the scene and BVH code build and test without a GPU.
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    float operator[](int axis) const { return axis == 0 ? x : (axis == 1 ? y : z); }
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(float s, Vec3 a) { return a * s; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a) { return a * (1.0f / length(a)); }
inline Vec3 vmin(Vec3 a, Vec3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline Vec3 vmax(Vec3 a, Vec3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }

// An axis-aligned bounding box. It starts "inside out" (min = +inf, max = -inf), so growing it
// by the first point makes it exactly that point, with no special case for "empty".
struct Aabb {
    Vec3 min{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::infinity()};
    Vec3 max{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()};

    void grow(Vec3 p) {
        min = vmin(min, p);
        max = vmax(max, p);
    }
    void grow(const Aabb& b) {
        min = vmin(min, b.min);
        max = vmax(max, b.max);
    }
    bool empty() const { return min.x > max.x; }
    Vec3 extent() const { return max - min; }
    // Surface area. SAH uses it as "how likely a random ray is to hit this box".
    // An empty box has area 0, so an empty side of a split costs nothing.
    float area() const {
        if (empty()) return 0.0f;
        Vec3 e = extent();
        return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
    }
};
