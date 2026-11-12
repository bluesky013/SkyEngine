#include <softbody/solver/CpuSoftBodySolver.h>

#include <physics/softbody/ISoftBodyCollisionQuery.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace sky::softbody {
    namespace {
        using namespace phy;
        constexpr double   CONTACT_TOLERANCE      = 0.00005;
        constexpr double   SOFT_RESIDUAL_LIMIT    = .0004;
        constexpr double   SOFT_CORRECTION_LIMIT  = .0005;
        constexpr uint32_t SOFT_CORRECTION_PASSES = 8;

        struct Timer {
            uint64_t                             &elapsed;
            bool                                  enabled;
            std::chrono::steady_clock::time_point start;
            Timer(uint64_t &value, bool active) : elapsed(value), enabled(active)
            {
                if (enabled)
                    start = std::chrono::steady_clock::now();
            }
            ~Timer()
            {
                if (enabled)
                    elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
            }
        };

        struct Vec {
            double x = 0, y = 0, z = 0;
            Vec() = default;
            Vec(double x, double y, double z) : x(x), y(y), z(z)
            {
            }
            explicit Vec(const Vector3 &v) : x(v.x), y(v.y), z(v.z)
            {
            }
            Vec operator+(const Vec &v) const
            {
                return {x + v.x, y + v.y, z + v.z};
            }
            Vec operator-(const Vec &v) const
            {
                return {x - v.x, y - v.y, z - v.z};
            }
            Vec operator-() const
            {
                return {-x, -y, -z};
            }
            Vec operator*(double s) const
            {
                return {x * s, y * s, z * s};
            }
            Vec operator/(double s) const
            {
                return *this * (1.0 / s);
            }
            double Dot(const Vec &v) const
            {
                return x * v.x + y * v.y + z * v.z;
            }
            Vec Cross(const Vec &v) const
            {
                return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x};
            }
            double Length() const
            {
                return std::sqrt(Dot(*this));
            }
            bool Finite() const
            {
                return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
            }
            bool Export(Vector3 &out) const
            {
                constexpr double limit = std::numeric_limits<float>::max();
                if (!Finite() || std::abs(x) > limit || std::abs(y) > limit || std::abs(z) > limit)
                    return false;
                out = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                return true;
            }
        };
        PhysicsStatus Invalid(const char *message)
        {
            return {PhysicsError::InvalidArgument, message};
        }
        PhysicsStatus Failed(const char *message)
        {
            return {PhysicsError::InvalidState, message};
        }
        bool Rigid(const Transform &root)
        {
            const auto &q = root.rotation;
            return Vec(root.translation).Finite() && root.scale == VEC3_ONE && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) &&
                   std::isfinite(q.w) && std::abs(q.Dot(q) - 1.f) <= .0001f;
        }
        double Volume(const Vec &a, const Vec &b, const Vec &c, const Vec &d)
        {
            return (b - a).Dot((c - a).Cross(d - a)) / 6.0;
        }
        PhysicsStatus CheckPositions(const SoftBodyMesh &mesh, const std::vector<Vec> &positions)
        {
            for (const auto &p : positions)
                if (!p.Finite())
                    return Failed("Nonfinite soft body position");
            for (const auto &tet : mesh.tetrahedra) {
                const auto  &v      = tet.vertices;
                const double volume = Volume(positions[v[0]], positions[v[1]], positions[v[2]], positions[v[3]]);
                if (!std::isfinite(volume) || volume <= 0.0)
                    return Failed("Soft body tetrahedron inverted or collapsed");
            }
            return {};
        }
        struct Contact {
            SoftBodyCollisionContact data;
            Vec                      normal, plane, velocity;
            double                   lambda = 0;
        };
        bool Same(const Contact &a, const Contact &b)
        {
            return a.data.object == b.data.object && a.data.feature == b.data.feature && a.normal.Dot(b.normal) > .9999 &&
                   std::abs((a.plane - b.plane).Dot(a.normal)) <= CONTACT_TOLERANCE;
        }
        PhysicsStatus Query(const ISoftBodyCollisionQuery &collision,
                            const SoftBodyCollisionQuery  &query,
                            const std::vector<Contact>    &previous,
                            std::vector<Contact>          &out,
                            SoftBodyStepMetrics           &metrics,
                            bool                           profile)
        {
            std::vector<SoftBodyCollisionContact> fresh;
            {
                const Timer timer(metrics.rigidQueryNs, profile);
                ++metrics.rigidQueries;
                if (const auto status = collision.QuerySoftBodyContacts(query, fresh); !status)
                    return status;
                metrics.rigidContacts += fresh.size();
            }
            if (fresh.size() > 64)
                return {PhysicsError::CapacityExceeded, "Soft body contact manifold exceeds 64 planes"};
            std::vector<Contact> contacts;
            for (const auto &value : fresh) {
                const Vec normal(value.normal), point(value.point), velocity(value.surfaceVelocity);
                if (!IsValid(value.object) || !normal.Finite() || !point.Finite() || !velocity.Finite() ||
                    std::abs(normal.Dot(normal) - 1.0) > .0001 || !std::isfinite(value.fraction) || value.fraction < 0 || value.fraction > 1)
                    return Invalid("Collision provider returned an invalid soft body contact");
                if (value.dynamic)
                    return {PhysicsError::Unsupported, "CPU soft body dynamic rigid coupling is not supported"};
                Contact contact{value, normal / normal.Length(), point, velocity, 0};
                for (const auto &old : previous)
                    if (Same(contact, old)) {
                        contact.lambda = old.lambda;
                        break;
                    }
                auto duplicate = std::find_if(contacts.begin(), contacts.end(), [&](const Contact &old) { return Same(contact, old); });
                if (duplicate == contacts.end())
                    contacts.emplace_back(std::move(contact));
                else if (contact.plane.Dot(contact.normal) > duplicate->plane.Dot(contact.normal))
                    *duplicate = std::move(contact);
            }
            out = std::move(contacts);
            return {};
        }

        // A closest feasible point in 3D needs at most three independent active planes.
        bool Project(Vec &position, double inverseMass, double radius, std::vector<Contact> &contacts)
        {
            if (contacts.empty())
                return true;
            if (inverseMass == 0) {
                return std::all_of(contacts.begin(), contacts.end(),
                                   [&](const Contact &c) { return (position - c.plane).Dot(c.normal) - radius >= -CONTACT_TOLERANCE; });
            }
            Vec reference = position;
            for (const auto &c : contacts)
                reference = reference - c.normal * (inverseMass * c.lambda);
            std::vector<double> offsets;
            for (const auto &c : contacts)
                offsets.push_back(radius - (reference - c.plane).Dot(c.normal));
            double                best = std::numeric_limits<double>::infinity();
            Vec                   bestPosition;
            std::array<size_t, 3> bestActive{};
            std::array<double, 3> bestLambda{};
            size_t                bestCount = 0;
            const auto            candidate = [&](std::array<size_t, 3> active, std::array<double, 3> lambda, size_t count) {
                Vec correction;
                for (size_t k = 0; k < count; ++k) {
                    if (!std::isfinite(lambda[k]) || lambda[k] < -1e-10)
                        return;
                    lambda[k]  = std::max(0.0, lambda[k]);
                    correction = correction + contacts[active[k]].normal * lambda[k];
                }
                const Vec    point    = reference + correction;
                const double distance = correction.Dot(correction);
                if (!point.Finite() || distance >= best)
                    return;
                for (const auto &c : contacts)
                    if ((point - c.plane).Dot(c.normal) - radius < -CONTACT_TOLERANCE)
                        return;
                best         = distance;
                bestPosition = point;
                bestActive   = active;
                bestLambda   = lambda;
                bestCount    = count;
            };
            candidate({}, {}, 0);
            if (best != 0.0) {
                for (size_t i = 0; i < contacts.size(); ++i)
                    candidate({i, 0, 0}, {offsets[i], 0, 0}, 1);
                for (size_t i = 0; i < contacts.size(); ++i)
                    for (size_t j = i + 1; j < contacts.size(); ++j) {
                        const Vec    a = contacts[i].normal, b = contacts[j].normal, cross = a.Cross(b);
                        const double determinant = cross.Dot(cross);
                        if (determinant <= 1e-20)
                            continue;
                        const Vec correction = (b.Cross(cross) * offsets[i] + cross.Cross(a) * offsets[j]) / determinant;
                        candidate({i, j, 0}, {correction.Cross(b).Dot(cross) / determinant, a.Cross(correction).Dot(cross) / determinant, 0}, 2);
                    }
                for (size_t i = 0; i < contacts.size(); ++i)
                    for (size_t j = i + 1; j < contacts.size(); ++j)
                        for (size_t k = j + 1; k < contacts.size(); ++k) {
                            const Vec    a = contacts[i].normal, b = contacts[j].normal, c = contacts[k].normal;
                            const Vec    bc = b.Cross(c), ca = c.Cross(a), ab = a.Cross(b);
                            const double determinant = a.Dot(bc);
                            if (std::abs(determinant) <= 1e-12)
                                continue;
                            const Vec correction = (bc * offsets[i] + ca * offsets[j] + ab * offsets[k]) / determinant;
                            candidate({i, j, k},
                                      {correction.Dot(bc) / determinant, correction.Dot(ca) / determinant, correction.Dot(ab) / determinant}, 3);
                        }
            }
            if (!std::isfinite(best))
                return false;
            position = bestPosition;
            for (auto &c : contacts)
                c.lambda = 0;
            for (size_t k = 0; k < bestCount; ++k)
                contacts[bestActive[k]].lambda = bestLambda[k] / inverseMass;
            return true;
        }
        struct BodyWork {
            CpuSoftBodySolver                *solver   = nullptr;
            const SoftBodyDesc               *desc     = nullptr;
            const Transform                  *root     = nullptr;
            const std::vector<double>        *edgeRest = nullptr, *volumeRest = nullptr;
            const std::vector<uint8_t>       *surface = nullptr;
            SoftBodySnapshot                  next;
            std::vector<Vec>                  positions, start, velocities, followLambda;
            std::vector<double>               mass, edgeLambda, volumeLambda;
            std::vector<std::vector<Contact>> contacts;
        };

        struct SoftContact {
            size_t                pointBody = 0, faceBody = 0;
            uint32_t              vertex = 0, face = 0;
            std::array<double, 3> weights{};
            Vec                   normal;
            double                lambda = 0;
            bool                  active = false;
        };

        struct SoftResidual {
            double   penetration = 0., violation = 0.;
            bool     eligible = true, centerInside = false;
            size_t   pointBody = 0, faceBody = 0;
            uint32_t vertex = 0, face = 0;
        };

        PhysicsStatus ResidualFailure(const SoftResidual &residual, uint64_t step, const char *reason, uint32_t passes, double correction)
        {
            std::ostringstream message;
            message << std::setprecision(9) << "Soft body final inter-body penetration remains: step=" << step << " reason=" << reason
                    << " residual_m=" << residual.penetration << " violation_m=" << residual.violation
                    << " qualification_limit_m=" << SOFT_RESIDUAL_LIMIT << " center_inside=" << residual.centerInside
                    << " eligible=" << residual.eligible << " passes=" << passes << " pass_limit=" << SOFT_CORRECTION_PASSES
                    << " exhausted=" << (passes == SOFT_CORRECTION_PASSES) << " correction_m=" << correction
                    << " correction_limit_m=" << SOFT_CORRECTION_LIMIT << " point_body=" << residual.pointBody << " face_body=" << residual.faceBody
                    << " vertex=" << residual.vertex << " face=" << residual.face;
            return {PhysicsError::InvalidState, message.str()};
        }

        struct Bounds {
            Vec low{INFINITY, INFINITY, INFINITY}, high{-INFINITY, -INFINITY, -INFINITY};
        };
        Bounds Bound(const BodyWork &body)
        {
            Bounds result;
            for (const auto &p : body.positions) {
                result.low  = {std::min(result.low.x, p.x), std::min(result.low.y, p.y), std::min(result.low.z, p.z)};
                result.high = {std::max(result.high.x, p.x), std::max(result.high.y, p.y), std::max(result.high.z, p.z)};
            }
            const double radius = body.desc->vertexRadius;
            result.low          = result.low - Vec{radius, radius, radius};
            result.high         = result.high + Vec{radius, radius, radius};
            return result;
        }
        std::vector<std::array<size_t, 2>> Pairs(const std::vector<BodyWork> &bodies, SoftBodyStepMetrics &metrics, bool profile)
        {
            const Timer         timer(metrics.softBroadphaseNs, profile);
            std::vector<Bounds> bounds;
            for (const auto &body : bodies)
                bounds.push_back(Bound(body));
            std::vector<std::array<size_t, 2>> pairs;
            for (size_t a = 0; a < bodies.size(); ++a)
                for (size_t b = a + 1; b < bodies.size(); ++b) {
                    ++metrics.broadphasePairs;
                    if (!FilterAccepts(bodies[a].desc->filter, bodies[b].desc->filter))
                        continue;
                    const auto &x = bounds[a], &y = bounds[b];
                    if (x.low.x > y.high.x || y.low.x > x.high.x || x.low.y > y.high.y || y.low.y > x.high.y || x.low.z > y.high.z ||
                        y.low.z > x.high.z)
                        continue;
                    ++metrics.broadphaseAccepted;
                    pairs.push_back({a, b});
                }
            return pairs;
        }

        bool ClosestTriangle(const Vec &p, const Vec &a, const Vec &b, const Vec &c, std::array<double, 3> &weights, Vec &point)
        {
            const Vec ab = b - a, ac = c - a, ap = p - a;
            if (ab.Cross(ac).Dot(ab.Cross(ac)) <= 1e-30)
                return false;
            const double d1 = ab.Dot(ap), d2 = ac.Dot(ap);
            if (d1 <= 0 && d2 <= 0)
                weights = {1, 0, 0};
            else {
                const Vec    bp = p - b;
                const double d3 = ab.Dot(bp), d4 = ac.Dot(bp), vc = d1 * d4 - d3 * d2;
                if (d3 >= 0 && d4 <= d3)
                    weights = {0, 1, 0};
                else if (vc <= 0 && d1 >= 0 && d3 <= 0) {
                    const double v = d1 / (d1 - d3);
                    weights        = {1 - v, v, 0};
                } else {
                    const Vec    cp = p - c;
                    const double d5 = ab.Dot(cp), d6 = ac.Dot(cp), vb = d5 * d2 - d1 * d6, va = d3 * d6 - d5 * d4;
                    if (d6 >= 0 && d5 <= d6)
                        weights = {0, 0, 1};
                    else if (vb <= 0 && d2 >= 0 && d6 <= 0) {
                        const double w = d2 / (d2 - d6);
                        weights        = {1 - w, 0, w};
                    } else if (va <= 0 && d4 >= d3 && d5 >= d6) {
                        const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                        weights        = {0, 1 - w, w};
                    } else {
                        const double denominator = va + vb + vc;
                        if (!std::isfinite(denominator) || denominator <= 0)
                            return false;
                        weights = {va / denominator, vb / denominator, vc / denominator};
                    }
                }
            }
            point = a * weights[0] + b * weights[1] + c * weights[2];
            return point.Finite();
        }
        bool Inside(const BodyWork &body, const Vec &point, SoftBodyStepMetrics &metrics)
        {
            for (const auto &tet : body.desc->mesh.tetrahedra) {
                ++metrics.tetContainmentTests;
                const auto                 &v = tet.vertices;
                const Vec                   a = body.positions[v[0]], b = body.positions[v[1]], c = body.positions[v[2]], d = body.positions[v[3]];
                const double                volume = Volume(a, b, c, d);
                const std::array<double, 4> bary{Volume(point, b, c, d) / volume, Volume(a, point, c, d) / volume, Volume(a, b, point, d) / volume,
                                                 Volume(a, b, c, point) / volume};
                if (std::all_of(bary.begin(), bary.end(), [](double value) { return value >= -1e-10 && value <= 1. + 1e-10; }))
                    return true;
            }
            return false;
        }
        PhysicsStatus Nearest(const BodyWork &body, const Vec &point, SoftContact &contact, double &distance, SoftBodyStepMetrics &metrics)
        {
            double best = INFINITY;
            Vec    nearest;
            for (uint32_t i = 0; i < body.desc->mesh.faces.size(); ++i) {
                ++metrics.vertexTriangleTests;
                const auto           &f = body.desc->mesh.faces[i];
                Vec                   q;
                std::array<double, 3> weights;
                if (!ClosestTriangle(point, body.positions[f[0]], body.positions[f[1]], body.positions[f[2]], weights, q))
                    return Failed("Soft body contact triangle degenerated");
                const double squared = (point - q).Dot(point - q);
                if (squared < best) {
                    best            = squared;
                    contact.face    = i;
                    contact.weights = weights;
                    nearest         = q;
                }
            }
            if (!std::isfinite(best))
                return Failed("Soft body has no finite contact boundary");
            distance          = std::sqrt(best);
            const bool inside = Inside(body, point, metrics);
            if (distance > 1e-12)
                contact.normal = (point - nearest) * ((inside ? -1. : 1.) / distance);
            else {
                const auto &f  = body.desc->mesh.faces[contact.face];
                contact.normal = (body.positions[f[1]] - body.positions[f[0]]).Cross(body.positions[f[2]] - body.positions[f[0]]);
                contact.normal = contact.normal / contact.normal.Length();
            }
            if (inside)
                distance = -distance;
            return {};
        }
        bool Same(const SoftContact &a, const SoftContact &b)
        {
            return a.pointBody == b.pointBody && a.faceBody == b.faceBody && a.vertex == b.vertex && a.face == b.face;
        }
        PhysicsStatus SoftPass(std::vector<BodyWork>    &bodies,
                               std::vector<SoftContact> &contacts,
                               double                    alpha,
                               SoftBodyStepMetrics      &metrics,
                               bool                      profile,
                               bool                      validate,
                               SoftResidual             *residual     = nullptr,
                               bool                      residualOnly = false)
        {
            if (!validate && !residualOnly)
                for (auto &contact : contacts)
                    contact.active = false;
            for (const auto &pair : Pairs(bodies, metrics, profile)) {
                for (size_t direction = 0; direction < 2; ++direction) {
                    const size_t a = pair[direction], b = pair[1 - direction];
                    auto        &pointBody = bodies[a], &faceBody = bodies[b];
                    const double radius = pointBody.desc->vertexRadius + faceBody.desc->vertexRadius;
                    for (uint32_t vertex = 0; vertex < pointBody.positions.size(); ++vertex) {
                        if (!(*pointBody.surface)[vertex])
                            continue;
                        SoftContact found;
                        found.pointBody = a;
                        found.faceBody  = b;
                        found.vertex    = vertex;
                        double distance = 0;
                        {
                            const Timer timer(metrics.softNarrowphaseNs, profile);
                            if (const auto status = Nearest(faceBody, pointBody.positions[vertex], found, distance, metrics); !status)
                                return status;
                        }
                        const double constraint = distance - radius;
                        auto         previous   = std::find_if(contacts.begin(), contacts.end(), [&](const auto &old) { return Same(old, found); });
                        if (residual) {
                            const double lambda = previous != contacts.end() ? previous->lambda : 0.;
                            if (-constraint > residual->penetration) {
                                residual->penetration = -constraint;
                                residual->pointBody   = a;
                                residual->faceBody    = b;
                                residual->vertex      = vertex;
                                residual->face        = found.face;
                            }
                            residual->violation    = std::max(residual->violation, -(constraint + alpha * lambda));
                            residual->centerInside = residual->centerInside || distance < -CONTACT_TOLERANCE;
                            residual->eligible     = !residual->centerInside && residual->penetration <= SOFT_RESIDUAL_LIMIT;
                        }
                        if (validate) {
                            const double lambda = previous != contacts.end() ? previous->lambda : 0.;
                            if (residual)
                                continue;
                            metrics.maxPenetration = std::max(metrics.maxPenetration, std::max(0., -constraint));
                            if (distance < -CONTACT_TOLERANCE || constraint + alpha * lambda < -CONTACT_TOLERANCE)
                                return Failed("Soft body final inter-body penetration remains");
                            continue;
                        }
                        if (residualOnly) {
                            const double lambda = previous != contacts.end() ? previous->lambda : 0.;
                            if (constraint + alpha * lambda >= -CONTACT_TOLERANCE)
                                continue;
                            if (distance < -CONTACT_TOLERANCE || -constraint > SOFT_RESIDUAL_LIMIT)
                                return Failed("Soft body final inter-body penetration remains");
                        }
                        if (constraint > CONTACT_TOLERANCE && (previous == contacts.end() || previous->lambda == 0))
                            continue;
                        const Timer timer(metrics.softProjectionNs, profile);
                        if (previous == contacts.end()) {
                            contacts.push_back(found);
                            previous = std::prev(contacts.end());
                            ++metrics.softContacts;
                        } else {
                            found.lambda = previous->lambda;
                            *previous    = found;
                        }
                        auto       &contact     = *previous;
                        const auto &face        = faceBody.desc->mesh.faces[contact.face];
                        double      denominator = pointBody.mass[vertex];
                        for (size_t k = 0; k < 3; ++k)
                            denominator += contact.weights[k] * contact.weights[k] * faceBody.mass[face[k]];
                        if (denominator == 0) {
                            if (constraint < -CONTACT_TOLERANCE)
                                return Failed("Soft body contact has no movable participants");
                            continue;
                        }
                        const double next  = std::max(0., contact.lambda + (-constraint - alpha * contact.lambda) / (denominator + alpha));
                        const double delta = next - contact.lambda;
                        contact.lambda     = next;
                        contact.active     = next > 0;
                        if (contact.active)
                            ++metrics.activeSoftContacts;
                        pointBody.positions[vertex] = pointBody.positions[vertex] + contact.normal * (pointBody.mass[vertex] * delta);
                        for (size_t k = 0; k < 3; ++k)
                            faceBody.positions[face[k]] =
                                faceBody.positions[face[k]] - contact.normal * (faceBody.mass[face[k]] * contact.weights[k] * delta);
                    }
                }
            }
            return {};
        }
        bool Crosses(const Vec &a, const Vec &b, const Vec &x, const Vec &y, const Vec &z)
        {
            const Vec    normal = (y - x).Cross(z - x), edge = b - a;
            const double denominator = normal.Dot(edge);
            if (std::abs(denominator) <= 1e-12 * normal.Length() * edge.Length())
                return false;
            const double t = normal.Dot(x - a) / denominator;
            if (t <= 1e-8 || t >= 1. - 1e-8)
                return false;
            const Vec             point = a + edge * t;
            std::array<double, 3> weights;
            Vec                   nearest;
            return ClosestTriangle(point, x, y, z, weights, nearest) && (nearest - point).Length() < 1e-8 &&
                   *std::min_element(weights.begin(), weights.end()) >= -1e-8;
        }
        PhysicsStatus ValidateCrossings(const std::vector<BodyWork> &bodies, SoftBodyStepMetrics &metrics, bool profile)
        {
            const auto  pairs = Pairs(bodies, metrics, profile);
            const Timer timer(metrics.softNarrowphaseNs, profile);
            for (const auto &pair : pairs)
                for (size_t direction = 0; direction < 2; ++direction) {
                    const auto &a = bodies[pair[direction]], &b = bodies[pair[1 - direction]];
                    for (const auto &faceA : a.desc->mesh.faces)
                        for (size_t e = 0; e < 3; ++e)
                            for (const auto &faceB : b.desc->mesh.faces) {
                                ++metrics.segmentTriangleTests;
                                if (Crosses(a.positions[faceA[e]], a.positions[faceA[(e + 1) % 3]], b.positions[faceB[0]], b.positions[faceB[1]],
                                            b.positions[faceB[2]]))
                                    return {PhysicsError::Unsupported,
                                            "Soft body edge/face crossing requires unsupported continuous or edge contact"};
                            }
                }
            return {};
        }
        void SoftVelocities(std::vector<BodyWork> &bodies, const std::vector<SoftContact> &contacts, double h)
        {
            for (const auto &contact : contacts) {
                if (!contact.active || contact.lambda <= 0)
                    continue;
                auto       &a = bodies[contact.pointBody], &b = bodies[contact.faceBody];
                const auto &face = b.desc->mesh.faces[contact.face];
                Vec         q, faceVelocity;
                double      denominator = a.mass[contact.vertex];
                for (size_t k = 0; k < 3; ++k) {
                    q            = q + b.positions[face[k]] * contact.weights[k];
                    faceVelocity = faceVelocity + b.velocities[face[k]] * contact.weights[k];
                    denominator += b.mass[face[k]] * contact.weights[k] * contact.weights[k];
                }
                if (denominator <= 0 ||
                    (a.positions[contact.vertex] - q).Dot(contact.normal) > a.desc->vertexRadius + b.desc->vertexRadius + CONTACT_TOLERANCE)
                    continue;
                const Vec    relative    = a.velocities[contact.vertex] - faceVelocity;
                const double normalSpeed = relative.Dot(contact.normal);
                const double restitution = std::min(a.desc->restitution, b.desc->restitution);
                const double impulse     = std::max(0., -(1. + restitution) * normalSpeed) / denominator;
                Vec          correction  = contact.normal * impulse;
                const Vec    tangent     = relative - contact.normal * normalSpeed;
                const double speed = tangent.Length(), friction = std::sqrt(static_cast<double>(a.desc->friction) * b.desc->friction);
                if (speed > 1e-14)
                    correction = correction - tangent * (std::min(speed / denominator, friction * std::max(impulse, contact.lambda / h)) / speed);
                a.velocities[contact.vertex] = a.velocities[contact.vertex] + correction * a.mass[contact.vertex];
                for (size_t k = 0; k < 3; ++k)
                    b.velocities[face[k]] = b.velocities[face[k]] - correction * (b.mass[face[k]] * contact.weights[k]);
            }
        }
    } // namespace

    phy::PhysicsStatus CpuSoftBodySolver::Initialize(const phy::SoftBodyDesc &input, const Uuid &id)
    {
        if (!id)
            return Invalid("Soft body snapshot identity is required");
        if (const auto status = phy::ValidateSoftBodyDesc(input); !status)
            return status;
        if (input.pressure != 0)
            return {phy::PhysicsError::Unsupported, "CPU soft body gas pressure is not supported"};
        CpuSoftBodySolver candidate;
        candidate.desc              = input;
        candidate.rootTarget        = input.transform;
        candidate.snapshot.id       = id;
        candidate.snapshot.revision = 1;
        candidate.surfaceVertices.resize(input.mesh.vertices.size(), 0);
        for (const auto &face : input.mesh.faces)
            for (const auto vertex : face)
                candidate.surfaceVertices[vertex] = 1;
        for (const auto &vertex : input.mesh.vertices) {
            const auto position = input.transform.Translate(vertex.position);
            const auto velocity = input.transform.rotation * vertex.velocity;
            if (!Vec(position).Finite() || !Vec(velocity).Finite())
                return Invalid("Initial soft body transform overflowed");
            candidate.snapshot.worldPositions.push_back(position);
            candidate.snapshot.worldVelocities.push_back(velocity);
        }
        std::vector<Vec> initialPositions;
        for (const auto &position : candidate.snapshot.worldPositions)
            initialPositions.emplace_back(position);
        if (const auto status = CheckPositions(input.mesh, initialPositions); !status)
            return status;
        for (const auto &edge : input.mesh.edges) {
            candidate.edgeRestLengths.push_back(
                (Vec(input.mesh.vertices[edge.vertices[0]].position) - Vec(input.mesh.vertices[edge.vertices[1]].position)).Length());
        }
        for (const auto &tet : input.mesh.tetrahedra) {
            const auto &v = tet.vertices;
            candidate.tetrahedronRestVolumes.push_back(Volume(Vec(input.mesh.vertices[v[0]].position), Vec(input.mesh.vertices[v[1]].position),
                                                              Vec(input.mesh.vertices[v[2]].position), Vec(input.mesh.vertices[v[3]].position)));
        }
        candidate.initialized = true;
        *this                 = std::move(candidate);
        return {};
    }
    phy::PhysicsStatus CpuSoftBodySolver::SetRootTarget(const Transform &root)
    {
        if (!initialized)
            return Failed("CPU soft body is not initialized");
        if (!Rigid(root))
            return Invalid("Soft body root target must be a finite rigid transform");
        rootTarget = root;
        return {};
    }

    phy::PhysicsStatus CpuSoftBodySolver::ReadParameters(phy::SoftBodyRuntimeParameters &out) const
    {
        if (!initialized)
            return Failed("CPU soft body is not initialized");
        const auto range = [](const auto &constraints) {
            phy::SoftBodyComplianceRange result;
            result.count = constraints.size();
            if (!constraints.empty()) {
                const auto [lo, hi] = std::minmax_element(constraints.begin(), constraints.end(),
                                                          [](const auto &a, const auto &b) { return a.compliance < b.compliance; });
                result.minimum      = lo->compliance;
                result.maximum      = hi->compliance;
            }
            return result;
        };
        phy::SoftBodyRuntimeParameters next;
        next.id               = snapshot.id;
        next.controlPoints    = static_cast<uint32_t>(desc.mesh.vertices.size());
        next.solverIterations = desc.solverIterations;
        next.pressure         = desc.pressure;
        next.linearDamping    = desc.linearDamping;
        next.followCompliance = range(desc.rootFollow);
        next.edgeCompliance   = range(desc.mesh.edges);
        next.volumeCompliance = range(desc.mesh.tetrahedra);
        out                   = next;
        return {};
    }

    phy::PhysicsStatus CpuSoftBodySolver::ApplyParameters(std::span<CpuSoftBodySolver *const> bodies, const phy::SoftBodyParameterPatch &patch)
    {
        if (bodies.empty() || (!patch.followCompliance && !patch.edgeCompliance && !patch.volumeCompliance && !patch.linearDamping))
            return Invalid("Soft body tuning requires live instances and at least one parameter");
        for (const auto &value : {patch.followCompliance, patch.edgeCompliance, patch.volumeCompliance, patch.linearDamping})
            if (value && (!std::isfinite(*value) || *value < 0.f))
                return Invalid("Soft body parameters must be finite and nonnegative");
        if (patch.followCompliance && *patch.followCompliance == 0.f)
            return Invalid("Dynamic root-follow compliance must be positive");
        for (size_t i = 0; i < bodies.size(); ++i) {
            const auto *body = bodies[i];
            if (!body || !body->initialized || std::find(bodies.begin(), bodies.begin() + i, body) != bodies.begin() + i)
                return Invalid("Soft body tuning requires distinct initialized instances");
            if ((patch.followCompliance && body->desc.rootFollow.empty()) || (patch.volumeCompliance && body->desc.mesh.tetrahedra.empty()))
                return {phy::PhysicsError::Unsupported, "Selected soft body lacks the requested constraints"};
        }
        // XPBD multipliers are per-step scratch. Only edit material coefficients, keeping rest data and the committed state intact.
        const auto replace = [](auto &constraints, const std::optional<float> &value) {
            if (value)
                for (auto &constraint : constraints)
                    constraint.compliance = *value;
        };
        for (auto *body : bodies) {
            auto &desc = body->desc;
            replace(desc.rootFollow, patch.followCompliance);
            replace(desc.mesh.edges, patch.edgeCompliance);
            replace(desc.mesh.tetrahedra, patch.volumeCompliance);
            if (patch.linearDamping)
                desc.linearDamping = *patch.linearDamping;
        }
        return {};
    }

    phy::PhysicsStatus CpuSoftBodySolver::Reset(const Transform &root)
    {
        if (!initialized)
            return Failed("CPU soft body is not initialized");
        if (!Rigid(root))
            return Invalid("Soft body reset requires a finite rigid transform");
        auto candidate = snapshot;
        for (size_t i = 0; i < desc.mesh.vertices.size(); ++i) {
            candidate.worldPositions[i] = root.Translate(desc.mesh.vertices[i].position);
            if (!Vec(candidate.worldPositions[i]).Finite())
                return Invalid("Soft body reset transform overflowed");
            candidate.worldVelocities[i] = VEC3_ZERO;
        }
        std::vector<Vec> resetPositions;
        for (const auto &position : candidate.worldPositions)
            resetPositions.emplace_back(position);
        if (const auto status = CheckPositions(desc.mesh, resetPositions); !status)
            return status;
        if (candidate.revision == UINT64_MAX)
            return Failed("Soft body revision exhausted");
        ++candidate.revision;
        rootTarget = root;
        snapshot   = std::move(candidate);
        return {};
    }

    phy::PhysicsStatus CpuSoftBodySolver::Step(float delta, const phy::ISoftBodyCollisionQuery &collision, uint64_t step)
    {
        const std::array<CpuSoftBodySolver *, 1> bodies{this};
        return StepBatch(bodies, delta, collision, step, {});
    }

    phy::PhysicsStatus CpuSoftBodySolver::StepBatch(std::span<CpuSoftBodySolver *const>    input,
                                                    float                                  delta,
                                                    const phy::ISoftBodyCollisionQuery    &collision,
                                                    uint64_t                               step,
                                                    const phy::SoftBodySimulationSettings &settings,
                                                    phy::SoftBodyStepMetrics              *metrics)
    {
        phy::SoftBodyStepMetrics stats;
        stats.step           = step;
        stats.attemptedSteps = 1;
        stats.bodyCount      = input.size();
        const bool profile   = settings.profileSteps && metrics;
        const auto run       = [&]() -> phy::PhysicsStatus {
            if (input.empty() || !std::isfinite(delta) || delta <= 0 || delta > .1f || !std::isfinite(settings.softContactCompliance) ||
                settings.softContactCompliance < 0)
                return Invalid("Soft body batch requires bodies, a finite interval in (0, 0.1] and nonnegative contact compliance");
            for (size_t i = 0; i < input.size(); ++i) {
                const auto *body = input[i];
                if (!body || !body->initialized)
                    return Failed("Soft body batch contains an uninitialized body");
                if (step <= body->snapshot.step || body->snapshot.revision == UINT64_MAX)
                    return Invalid("Soft body batch requires increasing step tokens and available revisions");
                for (size_t j = 0; j < i; ++j)
                    if (input[j] == body || input[j]->snapshot.id == body->snapshot.id)
                        return Invalid("Soft body batch contains duplicate bodies or identities");
            }
            const Vec gravity(collision.GetGravity());
            if (!gravity.Finite())
                return Invalid("Collision world gravity is nonfinite");
            const double          h = delta, h2 = h * h;
            uint32_t              iterations = 0;
            std::vector<BodyWork> bodies;
            bodies.reserve(input.size());
            {
                const Timer timer(stats.predictNs, profile);
                // The caller supplies stable contact traversal order; snapshot UUIDs identify instances only.
                for (auto *solver : input) {
                    BodyWork body;
                    body.solver      = solver;
                    body.desc        = &solver->desc;
                    body.root        = &solver->rootTarget;
                    body.edgeRest    = &solver->edgeRestLengths;
                    body.volumeRest  = &solver->tetrahedronRestVolumes;
                    body.surface     = &solver->surfaceVertices;
                    body.next        = solver->snapshot;
                    const auto &desc = solver->desc;
                    stats.particles += desc.mesh.vertices.size();
                    stats.surfaceVertices += std::count(solver->surfaceVertices.begin(), solver->surfaceVertices.end(), uint8_t{1});
                    stats.edges += desc.mesh.edges.size();
                    stats.tetrahedra += desc.mesh.tetrahedra.size();
                    stats.faces += desc.mesh.faces.size();
                    iterations = std::max(iterations, desc.solverIterations);
                    for (size_t i = 0; i < desc.mesh.vertices.size(); ++i) {
                        const auto &vertex = desc.mesh.vertices[i];
                        const Vec   initial(solver->snapshot.worldPositions[i]);
                        Vec         predicted = initial;
                        if (vertex.mode == phy::SoftBodyAttachmentMode::Dynamic) {
                            Vec          velocity = Vec(solver->snapshot.worldVelocities[i]) + gravity * (desc.gravityFactor * h);
                            const double speed    = velocity.Length();
                            if (!std::isfinite(speed))
                                return Failed("Soft body predicted velocity overflowed");
                            if (speed > desc.maxLinearVelocity)
                                velocity = velocity * (desc.maxLinearVelocity / speed);
                            predicted = initial + velocity * h;
                        } else if (vertex.mode == phy::SoftBodyAttachmentMode::RootKinematic)
                            predicted = Vec(solver->rootTarget.Translate(vertex.position));
                        body.start.push_back(initial);
                        body.positions.push_back(predicted);
                        body.mass.push_back(vertex.inverseMass);
                    }
                    if (const auto status = CheckPositions(desc.mesh, body.positions); !status)
                        return status;
                    body.edgeLambda.resize(desc.mesh.edges.size());
                    body.volumeLambda.resize(desc.mesh.tetrahedra.size());
                    body.followLambda.resize(desc.rootFollow.size());
                    body.contacts.resize(desc.mesh.vertices.size());
                    bodies.push_back(std::move(body));
                }
            }
            std::vector<SoftContact> softContacts;
            for (uint32_t iteration = 0; iteration < iterations; ++iteration) {
                for (auto &body : bodies) {
                    const auto &desc = *body.desc;
                    // Distribute each body's requested passes across the batch, including its final round.
                    if ((static_cast<uint64_t>(iteration) + 1) * desc.solverIterations / iterations ==
                        static_cast<uint64_t>(iteration) * desc.solverIterations / iterations)
                        continue;
                    const Timer timer(stats.internalXpbdNs, profile);
                    ++stats.internalIterations;
                    auto       &positions              = body.positions;
                    const auto &mass                   = body.mass;
                    auto       &edgeLambda             = body.edgeLambda;
                    auto       &volumeLambda           = body.volumeLambda;
                    auto       &followLambda           = body.followLambda;
                    const auto &edgeRestLengths        = *body.edgeRest;
                    const auto &tetrahedronRestVolumes = *body.volumeRest;
                    const auto &rootTarget             = *body.root;
                    for (size_t i = 0; i < desc.mesh.edges.size(); ++i) {
                        const auto  &edge = desc.mesh.edges[i];
                        const auto   a = edge.vertices[0], b = edge.vertices[1];
                        const Vec    difference = positions[a] - positions[b];
                        const double length = difference.Length(), alpha = edge.compliance / h2, denominator = mass[a] + mass[b] + alpha;
                        if (!std::isfinite(length))
                            return Failed("Soft body edge overflowed");
                        if (denominator == 0)
                            continue;
                        if (length <= 1e-14)
                            return Failed("Soft body edge collapsed");
                        const double dl = (-(length - edgeRestLengths[i]) - alpha * edgeLambda[i]) / denominator;
                        edgeLambda[i] += dl;
                        positions[a] = positions[a] + difference * (mass[a] * dl / length);
                        positions[b] = positions[b] - difference * (mass[b] * dl / length);
                    }
                    for (size_t i = 0; i < desc.mesh.tetrahedra.size(); ++i) {
                        const auto        &tet = desc.mesh.tetrahedra[i];
                        const auto        &v   = tet.vertices;
                        const Vec          a = positions[v[0]], b = positions[v[1]], c = positions[v[2]], d = positions[v[3]];
                        std::array<Vec, 4> gradient;
                        gradient[1]              = (c - a).Cross(d - a) / 6.0;
                        gradient[2]              = (d - a).Cross(b - a) / 6.0;
                        gradient[3]              = (b - a).Cross(c - a) / 6.0;
                        gradient[0]              = -(gradient[1] + gradient[2] + gradient[3]);
                        const double alpha       = tet.compliance / h2;
                        double       denominator = alpha;
                        for (size_t k = 0; k < 4; ++k)
                            denominator += mass[v[k]] * gradient[k].Dot(gradient[k]);
                        if (denominator == 0)
                            continue;
                        if (!std::isfinite(denominator) || denominator < 1e-30)
                            return Failed("Soft body volume gradient degenerated");
                        const double dl = (-(Volume(a, b, c, d) - tetrahedronRestVolumes[i]) - alpha * volumeLambda[i]) / denominator;
                        volumeLambda[i] += dl;
                        for (size_t k = 0; k < 4; ++k)
                            positions[v[k]] = positions[v[k]] + gradient[k] * (mass[v[k]] * dl);
                    }
                    for (size_t i = 0; i < desc.rootFollow.size(); ++i) {
                        const auto  &follow = desc.rootFollow[i];
                        const auto   index  = follow.vertex;
                        const Vec    target(rootTarget.Translate(desc.mesh.vertices[index].position));
                        const double alpha = follow.compliance / h2;
                        const Vec    dl    = (-(positions[index] - target) - followLambda[i] * alpha) / (mass[index] + alpha);
                        followLambda[i]    = followLambda[i] + dl;
                        positions[index]   = positions[index] + dl * mass[index];
                    }
                }
                if (settings.softCollision) {
                    for (const auto &body : bodies)
                        if (const auto status = CheckPositions(body.desc->mesh, body.positions); !status)
                            return status;
                    if (const auto status = SoftPass(bodies, softContacts, settings.softContactCompliance / h2, stats, profile, false); !status)
                        return status;
                }
                for (auto &body : bodies) {
                    const auto &desc      = *body.desc;
                    const bool  localPass = (static_cast<uint64_t>(iteration) + 1) * desc.solverIterations / iterations !=
                                           static_cast<uint64_t>(iteration) * desc.solverIterations / iterations;
                    if (settings.rigidCollision && (settings.softCollision || localPass)) {
                        for (size_t i = 0; i < body.positions.size(); ++i) {
                            if (!(*body.surface)[i])
                                continue;
                            phy::SoftBodyCollisionQuery query;
                            query.start = body.next.worldPositions[i];
                            if (!body.positions[i].Export(query.end))
                                return Failed("Soft body collision query overflowed");
                            query.radius    = desc.vertexRadius;
                            query.deltaTime = delta;
                            query.step      = step;
                            query.filter    = desc.filter;
                            std::vector<Contact> fresh;
                            if (const auto status = Query(collision, query, body.contacts[i], fresh, stats, profile); !status)
                                return status;
                            body.contacts[i] = std::move(fresh);
                            const Timer timer(stats.rigidProjectionNs, profile);
                            if (!Project(body.positions[i], body.mass[i], desc.vertexRadius, body.contacts[i]))
                                return Failed("Soft body contact planes are infeasible");
                            for (const auto &contact : body.contacts[i])
                                stats.activeRigidContacts += contact.lambda > 0;
                        }
                    }
                    if (const auto status = CheckPositions(desc.mesh, body.positions); !status)
                        return status;
                }
            }
            const auto finalize = [&]() -> PhysicsStatus {
                for (auto &body : bodies) {
                    const auto  &desc    = *body.desc;
                    const double damping = std::exp(-desc.linearDamping * h);
                    body.velocities.resize(body.positions.size());
                    for (size_t i = 0; i < body.positions.size(); ++i) {
                        {
                            const Timer timer(stats.finalizeNs, profile);
                            if (!body.positions[i].Export(body.next.worldPositions[i]))
                                return Failed("Soft body final position overflowed");
                            body.positions[i] = Vec(body.next.worldPositions[i]);
                        }
                        const Vec final = body.positions[i];
                        if (settings.rigidCollision && (*body.surface)[i]) {
                            Vector3 start;
                            body.start[i].Export(start);
                            phy::SoftBodyCollisionQuery query{start, body.next.worldPositions[i], desc.vertexRadius, delta, step, desc.filter};
                            std::vector<Contact>        finalContacts;
                            if (const auto status = Query(collision, query, body.contacts[i], finalContacts, stats, profile); !status)
                                return status;
                            const Timer timer(stats.rigidProjectionNs, profile);
                            for (const auto &contact : finalContacts) {
                                const double penetration = desc.vertexRadius - (final - contact.plane).Dot(contact.normal);
                                stats.maxPenetration     = std::max(stats.maxPenetration, std::max(0., penetration));
                                if (penetration > CONTACT_TOLERANCE)
                                    return Failed("Soft body final contact penetration remains");
                            }
                        }
                        const Timer timer(stats.finalizeNs, profile);
                        Vec         velocity = (final - body.start[i]) / h;
                        if (desc.mesh.vertices[i].mode == phy::SoftBodyAttachmentMode::Dynamic) {
                            velocity = velocity * damping;
                            for (const auto &contact : body.contacts[i]) {
                                if (contact.lambda <= 0 || (final - contact.plane).Dot(contact.normal) - desc.vertexRadius > CONTACT_TOLERANCE)
                                    continue;
                                Vec          relative       = velocity - contact.velocity;
                                const double normalVelocity = relative.Dot(contact.normal);
                                const double impulse        = std::max(0.0, -(1.0 + desc.restitution) * normalVelocity);
                                relative                    = relative + contact.normal * impulse;
                                const Vec    tangent        = relative - contact.normal * relative.Dot(contact.normal);
                                const double speed          = tangent.Length();
                                const double budget         = desc.friction * std::max(impulse, contact.lambda * body.mass[i] / h);
                                if (speed > 1e-14)
                                    relative = relative - tangent * (std::min(speed, budget) / speed);
                                velocity = relative + contact.velocity;
                            }
                        }
                        body.velocities[i] = velocity;
                    }
                    if (const auto status = CheckPositions(desc.mesh, body.positions); !status)
                        return status;
                }
                return {};
            };
            if (const auto status = finalize(); !status)
                return status;
            if (settings.softCollision) {
                auto status = SoftPass(bodies, softContacts, settings.softContactCompliance / h2, stats, profile, true);
                if (!status) {
                    std::vector<std::vector<Vec>> before;
                    for (const auto &body : bodies)
                        before.push_back(body.positions);
                    double     largestCorrection = 0.;
                    const auto bounded           = [&]() {
                        for (size_t b = 0; b < bodies.size(); ++b)
                            for (size_t i = 0; i < bodies[b].positions.size(); ++i) {
                                const Vec movement = bodies[b].positions[i] - before[b][i];
                                if (!movement.Finite()) {
                                    largestCorrection = INFINITY;
                                    return false;
                                }
                                const double squared = movement.Dot(movement);
                                largestCorrection    = std::max(largestCorrection, std::sqrt(squared));
                                if (squared > SOFT_CORRECTION_LIMIT * SOFT_CORRECTION_LIMIT)
                                    return false;
                            }
                        return true;
                    };
                    double worstPenetration = stats.maxPenetration;
                    bool   recovered        = false;
                    for (uint32_t pass = 0; pass < SOFT_CORRECTION_PASSES; ++pass) {
                        SoftResidual residual;
                        if (const auto audit = SoftPass(bodies, softContacts, settings.softContactCompliance / h2, stats, profile, true, &residual);
                            !audit)
                            return audit;
                        worstPenetration = std::max(worstPenetration, residual.penetration);
                        // Audit the whole batch before allowing small shell corrections, never center containment.
                        if (!residual.eligible || residual.violation <= CONTACT_TOLERANCE) {
                            stats.maxPenetration = worstPenetration;
                            const char *reason   = "validation";
                            if (residual.centerInside)
                                reason = "center_inside";
                            else if (residual.penetration > SOFT_RESIDUAL_LIMIT)
                                reason = "shell_limit";
                            return ResidualFailure(residual, step, reason, pass, largestCorrection);
                        }
                        if (const auto projection =
                                SoftPass(bodies, softContacts, settings.softContactCompliance / h2, stats, profile, false, &residual, true);
                            !projection)
                            return ResidualFailure(residual, step, "projection", pass + 1, largestCorrection);
                        if (!bounded())
                            return ResidualFailure(residual, step, "displacement_limit", pass + 1, largestCorrection);
                        for (auto &body : bodies) {
                            const auto &desc = *body.desc;
                            if (settings.rigidCollision) {
                                for (size_t i = 0; i < body.positions.size(); ++i) {
                                    if (!(*body.surface)[i])
                                        continue;
                                    SoftBodyCollisionQuery query;
                                    if (!body.start[i].Export(query.start) || !body.positions[i].Export(query.end))
                                        return Failed("Soft body residual collision query overflowed");
                                    query.radius    = desc.vertexRadius;
                                    query.deltaTime = delta;
                                    query.step      = step;
                                    query.filter    = desc.filter;
                                    std::vector<Contact> fresh;
                                    if (const auto queried = Query(collision, query, body.contacts[i], fresh, stats, profile); !queried)
                                        return queried;
                                    body.contacts[i] = std::move(fresh);
                                    const Timer timer(stats.rigidProjectionNs, profile);
                                    if (!Project(body.positions[i], body.mass[i], desc.vertexRadius, body.contacts[i]))
                                        return Failed("Soft body contact planes are infeasible");
                                    for (const auto &contact : body.contacts[i])
                                        stats.activeRigidContacts += contact.lambda > 0;
                                }
                            }
                            if (const auto valid = CheckPositions(desc.mesh, body.positions); !valid)
                                return valid;
                        }
                        if (!bounded())
                            return ResidualFailure(residual, step, "displacement_limit", pass + 1, largestCorrection);
                        // Final penetration describes the accepted candidate; counters and timers retain every retry.
                        stats.maxPenetration = 0.;
                        if (const auto finalized = finalize(); !finalized)
                            return finalized;
                        if (!bounded())
                            return ResidualFailure(residual, step, "displacement_limit", pass + 1, largestCorrection);
                        status = SoftPass(bodies, softContacts, settings.softContactCompliance / h2, stats, profile, true);
                        if (status) {
                            recovered = true;
                            break;
                        }
                        worstPenetration = std::max(worstPenetration, stats.maxPenetration);
                    }
                    if (!recovered) {
                        SoftResidual residual;
                        if (const auto audit = SoftPass(bodies, softContacts, settings.softContactCompliance / h2, stats, profile, true, &residual);
                            !audit)
                            return audit;
                        stats.maxPenetration = std::max(worstPenetration, residual.penetration);
                        return ResidualFailure(residual, step, "iteration_limit", SOFT_CORRECTION_PASSES, largestCorrection);
                    }
                }
                if (const auto status = ValidateCrossings(bodies, stats, profile); !status)
                    return status;
                const Timer timer(stats.softProjectionNs, profile);
                SoftVelocities(bodies, softContacts, h);
            }
            const Timer timer(stats.finalizeNs, profile);
            for (auto &body : bodies) {
                for (size_t i = 0; i < body.velocities.size(); ++i) {
                    auto velocity = body.velocities[i];
                    if (body.desc->mesh.vertices[i].mode == phy::SoftBodyAttachmentMode::Dynamic) {
                        const double speed = velocity.Length();
                        if (speed > body.desc->maxLinearVelocity)
                            velocity = velocity * (body.desc->maxLinearVelocity / speed);
                    }
                    if (!velocity.Export(body.next.worldVelocities[i]))
                        return Failed("Soft body final velocity overflowed");
                }
                body.next.step = step;
                ++body.next.revision;
            }
            for (auto &body : bodies)
                body.solver->snapshot = std::move(body.next);
            return {};
        };
        phy::PhysicsStatus result;
        {
            const Timer timer(stats.totalNs, profile);
            result = run();
        }
        stats.committed      = static_cast<bool>(result);
        stats.completedSteps = stats.committed ? 1 : 0;
        stats.failure        = result.error;
        if (metrics)
            *metrics = stats;
        return result;
    }
    phy::SoftBodySnapshot CpuSoftBodySolver::GetSnapshot() const
    {
        return snapshot;
    }
} // namespace sky::softbody
