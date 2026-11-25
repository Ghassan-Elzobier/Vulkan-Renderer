#include "scene/mesh.h"

#include <array>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

void appendQuad(Mesh& mesh, Vec3 a, Vec3 b, Vec3 c, Vec3 d, uint32_t material) {
    mesh.triangles.push_back({a, b, c, material});
    mesh.triangles.push_back({a, c, d, material});
}

void appendBox(Mesh& mesh, Vec3 lo, Vec3 hi, uint32_t material) {
    // The eight corners, indexed by bits: bit 0 picks x, bit 1 y, bit 2 z.
    auto corner = [&](int i) {
        return Vec3{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z};
    };
    const int faces[6][4] = {
        {0, 2, 3, 1}, {4, 5, 7, 6}, // -z, +z
        {0, 1, 5, 4}, {2, 6, 7, 3}, // -y, +y
        {0, 4, 6, 2}, {1, 3, 7, 5}, // -x, +x
    };
    for (const auto& f : faces) {
        appendQuad(mesh, corner(f[0]), corner(f[1]), corner(f[2]), corner(f[3]), material);
    }
}

void appendIcosphere(Mesh& mesh, Vec3 center, float radius, int subdivisions, uint32_t material) {
    // The 12 corners of an icosahedron sit on three golden rectangles.
    const float g = (1.0f + std::sqrt(5.0f)) / 2.0f;
    std::vector<Vec3> verts = {
        {-1, g, 0}, {1, g, 0}, {-1, -g, 0}, {1, -g, 0},
        {0, -1, g}, {0, 1, g}, {0, -1, -g}, {0, 1, -g},
        {g, 0, -1}, {g, 0, 1}, {-g, 0, -1}, {-g, 0, 1},
    };
    for (Vec3& v : verts) v = normalize(v);

    std::vector<std::array<uint32_t, 3>> faces = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1},
    };

    for (int s = 0; s < subdivisions; s++) {
        // Each edge's midpoint is made once and shared by both faces on that edge,
        // so the sphere has no cracks.
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpoints;
        auto midpoint = [&](uint32_t a, uint32_t b) {
            auto key = std::minmax(a, b);
            auto it = midpoints.find(key);
            if (it != midpoints.end()) return it->second;
            verts.push_back(normalize((verts[a] + verts[b]) * 0.5f));
            uint32_t index = uint32_t(verts.size() - 1);
            midpoints[key] = index;
            return index;
        };
        std::vector<std::array<uint32_t, 3>> next;
        next.reserve(faces.size() * 4);
        for (const auto& f : faces) {
            uint32_t ab = midpoint(f[0], f[1]);
            uint32_t bc = midpoint(f[1], f[2]);
            uint32_t ca = midpoint(f[2], f[0]);
            next.push_back({f[0], ab, ca});
            next.push_back({f[1], bc, ab});
            next.push_back({f[2], ca, bc});
            next.push_back({ab, bc, ca});
        }
        faces = std::move(next);
    }

    for (const auto& f : faces) {
        mesh.triangles.push_back({center + verts[f[0]] * radius, center + verts[f[1]] * radius,
                                  center + verts[f[2]] * radius, material});
    }
}

void loadObj(Mesh& mesh, const std::string& path, uint32_t material) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open OBJ '" + path + "'");
    }

    std::vector<Vec3> positions;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "v") {
            Vec3 p;
            in >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (tag == "f") {
            // A corner is "v", "v/vt", "v//vn" or "v/vt/vn"; only v matters here.
            // Negative indices count back from the last position read so far.
            std::vector<uint32_t> corners;
            std::string corner;
            while (in >> corner) {
                long index = std::stol(corner.substr(0, corner.find('/')));
                index = index < 0 ? long(positions.size()) + index : index - 1;
                if (index < 0 || index >= long(positions.size())) {
                    throw std::runtime_error("OBJ face index out of range in '" + path + "'");
                }
                corners.push_back(uint32_t(index));
            }
            for (size_t i = 1; i + 1 < corners.size(); i++) {
                mesh.triangles.push_back(
                    {positions[corners[0]], positions[corners[i]], positions[corners[i + 1]], material});
            }
        }
    }
}

void fitToBox(Mesh& mesh, size_t first, Vec3 base, float height) {
    Aabb bounds;
    for (size_t i = first; i < mesh.triangles.size(); i++) bounds.grow(mesh.triangles[i].bounds());
    if (bounds.empty()) return;

    const Vec3 e = bounds.extent();
    const float scale = height / std::max({e.x, e.y, e.z});
    const Vec3 from{(bounds.min.x + bounds.max.x) * 0.5f, bounds.min.y, (bounds.min.z + bounds.max.z) * 0.5f};
    auto place = [&](Vec3 p) { return base + (p - from) * scale; };
    for (size_t i = first; i < mesh.triangles.size(); i++) {
        Triangle& t = mesh.triangles[i];
        t.v0 = place(t.v0);
        t.v1 = place(t.v1);
        t.v2 = place(t.v2);
    }
}

Mesh makeTriangleScene() {
    Mesh mesh;
    const uint32_t ground = mesh.addMaterial({{0.5f, 0.5f, 0.5f}, kLambertian, 0.0f});
    const uint32_t chrome = mesh.addMaterial({{0.913f, 0.921f, 0.925f}, kMetal, 0.01f});
    const uint32_t red = mesh.addMaterial({{0.8f, 0.25f, 0.25f}, kLambertian, 0.0f});
    const uint32_t gold = mesh.addMaterial({{0.85f, 0.65f, 0.2f}, kMetal, 0.3f});

    // The ground is the plane y = -0.5, like the top of the big sphere in the spheres scene.
    appendQuad(mesh, {-20, -0.5f, 20}, {20, -0.5f, 20}, {20, -0.5f, -20}, {-20, -0.5f, -20}, ground);
    appendIcosphere(mesh, {0.0f, 0.4f, -2.0f}, 0.9f, 2, chrome); // 320 triangles, a bit faceted
    appendIcosphere(mesh, {-0.6f, -0.28f, -1.0f}, 0.22f, 1, red); // 80 triangles, clearly low-poly
    appendBox(mesh, {0.4f, -0.5f, -1.0f}, {0.7f, -0.2f, -0.7f}, gold); // 12 triangles
    return mesh;
}

Mesh makeObjScene(const std::string& objPath) {
    Mesh mesh;
    const uint32_t ground = mesh.addMaterial({{0.5f, 0.5f, 0.5f}, kLambertian, 0.0f});
    const uint32_t model = mesh.addMaterial({{0.7f, 0.7f, 0.7f}, kLambertian, 0.0f});
    appendQuad(mesh, {-20, -0.5f, 20}, {20, -0.5f, 20}, {20, -0.5f, -20}, {-20, -0.5f, -20}, ground);
    const size_t first = mesh.triangles.size();
    loadObj(mesh, objPath, model);
    fitToBox(mesh, first, {0.0f, -0.5f, -2.0f}, 1.6f);
    return mesh;
}

std::vector<GpuTriangle> packTriangles(const std::vector<Triangle>& triangles) {
    std::vector<GpuTriangle> out(triangles.size());
    for (size_t i = 0; i < triangles.size(); i++) {
        const Triangle& t = triangles[i];
        out[i] = GpuTriangle{{t.v0.x, t.v0.y, t.v0.z}, t.material, {t.v1.x, t.v1.y, t.v1.z}, 0.0f,
                             {t.v2.x, t.v2.y, t.v2.z}, 0.0f};
    }
    return out;
}

std::vector<GpuMaterial> packMaterials(const std::vector<Material>& materials) {
    std::vector<GpuMaterial> out(materials.size());
    for (size_t i = 0; i < materials.size(); i++) {
        const Material& m = materials[i];
        out[i] = GpuMaterial{{m.albedo.x, m.albedo.y, m.albedo.z}, m.fuzz, m.type, {0, 0, 0}};
    }
    return out;
}
