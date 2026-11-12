#include <physics/softbody/SoftBodyTypes.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace sky::phy {
    namespace {
        bool Finite(const Vector3 &v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }
        bool Nonnegative(float value)
        {
            return std::isfinite(value) && value >= 0;
        }
        PhysicsStatus Invalid(const char *message)
        {
            return {PhysicsError::InvalidArgument, message};
        }
        double Determinant(const Vector3 &a, const Vector3 &b, const Vector3 &c)
        {
            return double(a.x) * (double(b.y) * c.z - double(b.z) * c.y) - double(a.y) * (double(b.x) * c.z - double(b.z) * c.x) +
                   double(a.z) * (double(b.x) * c.y - double(b.y) * c.x);
        }
    } // namespace

    PhysicsStatus ValidateSoftBodyDesc(const SoftBodyDesc &desc)
    {
        const auto &q = desc.transform.rotation;
        if (!Finite(desc.transform.translation) || desc.transform.scale != VEC3_ONE || !std::isfinite(q.x) || !std::isfinite(q.y) ||
            !std::isfinite(q.z) || !std::isfinite(q.w) || std::abs(q.Dot(q) - 1.f) > .0001f)
            return Invalid("Soft body root requires a finite rigid transform");
        if (!desc.solverIterations || desc.solverIterations > 64 || !Nonnegative(desc.pressure) || !Nonnegative(desc.gravityFactor) ||
            !Nonnegative(desc.linearDamping) || !Nonnegative(desc.vertexRadius) || desc.vertexRadius == 0 || !Nonnegative(desc.friction) ||
            !Nonnegative(desc.restitution) || desc.restitution > 1 || !std::isfinite(desc.maxLinearVelocity) || desc.maxLinearVelocity <= 0)
            return Invalid("Invalid soft body material or solver settings");
        const auto &mesh = desc.mesh;
        if (mesh.vertices.size() < 3 || mesh.vertices.size() > std::numeric_limits<uint32_t>::max() || mesh.edges.empty() || mesh.faces.empty())
            return Invalid("Soft body requires indexed vertices, edges and surface faces");
        bool dynamic = false;
        for (const auto &v : mesh.vertices) {
            if (!Finite(v.position) || !Finite(v.velocity) || !Nonnegative(v.inverseMass) || v.mode >= SoftBodyAttachmentMode::END)
                return Invalid("Invalid soft body vertex or attachment mode");
            if (v.mode == SoftBodyAttachmentMode::Dynamic) {
                if (v.inverseMass <= 0)
                    return Invalid("Dynamic vertices require positive inverse mass");
                dynamic = true;
            } else if (v.inverseMass != 0 || v.velocity != VEC3_ZERO) {
                return Invalid("Attached vertices require zero inverse mass and initial velocity");
            }
        }
        if (!dynamic)
            return Invalid("Soft body requires a dynamic vertex");
        std::set<uint32_t> followed;
        for (const auto &follow : desc.rootFollow) {
            if (follow.vertex >= mesh.vertices.size() || mesh.vertices[follow.vertex].mode != SoftBodyAttachmentMode::Dynamic ||
                !std::isfinite(follow.compliance) || follow.compliance <= 0 || !followed.insert(follow.vertex).second)
                return Invalid("Root follow requires unique dynamic vertices and finite positive compliance");
        }
        const auto                        valid = [&mesh](uint32_t index) { return index < mesh.vertices.size(); };
        std::set<std::array<uint32_t, 2>> edges;
        for (const auto &e : mesh.edges) {
            auto sorted = e.vertices;
            std::sort(sorted.begin(), sorted.end());
            if (!valid(sorted[1]) || sorted[0] == sorted[1] || !Nonnegative(e.compliance) || !edges.insert(sorted).second)
                return Invalid("Invalid or duplicate soft body edge");
            const auto delta = mesh.vertices[sorted[0]].position - mesh.vertices[sorted[1]].position;
            if (!Finite(delta) || !std::isfinite(delta.Dot(delta)) || delta.Dot(delta) <= 1e-16f)
                return Invalid("Soft body edges require finite positive rest lengths");
        }
        std::set<std::array<uint32_t, 4>> tets;
        for (const auto &t : mesh.tetrahedra) {
            auto sorted = t.vertices;
            std::sort(sorted.begin(), sorted.end());
            if (!valid(sorted[3]) || std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end() || !Nonnegative(t.compliance) ||
                !tets.insert(sorted).second)
                return Invalid("Invalid or duplicate soft body tetrahedron");
            const auto  &p       = mesh.vertices[t.vertices[0]].position;
            const double volume6 = Determinant(mesh.vertices[t.vertices[1]].position - p, mesh.vertices[t.vertices[2]].position - p,
                                               mesh.vertices[t.vertices[3]].position - p);
            if (!std::isfinite(volume6) || volume6 <= 1e-12 || volume6 > std::numeric_limits<float>::max())
                return Invalid("Soft body tetrahedra require positive finite signed rest volume");
        }
        std::set<std::array<uint32_t, 3>>                           faces;
        std::map<std::array<uint32_t, 2>, std::pair<uint32_t, int>> incidence;
        double                                                      surfaceVolume6 = 0;
        for (const auto &face : mesh.faces) {
            auto sorted = face;
            std::sort(sorted.begin(), sorted.end());
            if (!valid(sorted[2]) || std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end() || !faces.insert(sorted).second)
                return Invalid("Invalid or duplicate soft body face");
            const auto &a      = mesh.vertices[face[0]].position;
            const auto &b      = mesh.vertices[face[1]].position;
            const auto &c      = mesh.vertices[face[2]].position;
            const auto  normal = (b - a).Cross(c - a);
            if (!Finite(normal) || !std::isfinite(normal.Dot(normal)) || normal.Dot(normal) <= 1e-16f)
                return Invalid("Soft body faces require nondegenerate geometry");
            surfaceVolume6 += Determinant(a, b, c);
            for (uint32_t i = 0; i < 3; ++i) {
                const auto first = face[i], second = face[(i + 1) % 3];
                auto      &use = incidence[{std::min(first, second), std::max(first, second)}];
                ++use.first;
                use.second += first < second ? 1 : -1;
            }
        }
        if (desc.pressure > 0 &&
            (!std::isfinite(surfaceVolume6) || surfaceVolume6 <= 1e-12 ||
             std::any_of(incidence.begin(), incidence.end(), [](const auto &entry) { return entry.second.first != 2 || entry.second.second != 0; })))
            return Invalid("Pressure requires a closed consistently outward oriented surface");
        return {};
    }
} // namespace sky::phy
