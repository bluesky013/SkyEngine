#include <softbody/authoring/SoftBodyRigGenerator.h>

#include "Geometry.h"
#include "SupportCage.h"

#include <bit>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace sky::softbody {
    namespace {
        using namespace detail;
        using Edge   = std::array<uint32_t, 2>;
        using Status = phy::PhysicsStatus;
        using Error  = phy::PhysicsError;
        struct Surface {
            Points           points;
            std::vector<Tri> faces;
            std::vector<D3>  normals;
            D3               low, high, center;
            double           scale = 0., volume = 0.;
        };
        struct Cone {
            Tri    face;
            Points rows;
        };
        D3 ToDouble(Vector3 p)
        {
            return {p.x, p.y, p.z};
        }
        Vector3 ToFloat(D3 p)
        {
            return {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
        }
        Edge SortedEdge(uint32_t a, uint32_t b)
        {
            return a < b ? Edge{a, b} : Edge{b, a};
        }
        bool Cancelled(const std::atomic_bool *cancel)
        {
            return cancel && cancel->load(std::memory_order_relaxed);
        }
        Status CancelStatus()
        {
            return {Error::InvalidState, "Softbody rig generation cancelled"};
        }
        Status Invalid(std::string message)
        {
            return {Error::InvalidArgument, std::move(message)};
        }
        bool Has(Tri face, uint32_t vertex)
        {
            return std::find(face.begin(), face.end(), vertex) != face.end();
        }

        Status LoadSurface(const RigGenerationInput &input, Surface &surface, size_t &bodyIndex)
        {
            const auto &settings = input.settings;
            const bool  supplied = settings.mode == RigGenerationMode::SuppliedCenterFan;
            if (settings.maxControlPoints < MIN_RIG_CONTROL_POINTS || settings.maxControlPoints > MAX_RIG_CONTROL_POINTS ||
                !std::isfinite(settings.maxSurfaceDistance) || settings.maxSurfaceDistance <= 0.f || !std::isfinite(settings.maxVolumeExcess) ||
                settings.maxVolumeExcess < 0.f || !std::isfinite(settings.minTetQuality) || settings.minTetQuality <= 0.f ||
                settings.minTetQuality > 1.f || static_cast<uint32_t>(settings.mode) >= static_cast<uint32_t>(RigGenerationMode::END) ||
                !std::isfinite(settings.bindingPositionScale) || settings.bindingPositionScale <= 0.f || settings.bindingPositionScale > 1000.f ||
                (supplied ? (input.controls.size() < 5 || input.controls.size() > settings.maxControlPoints || settings.fillControlBudget ||
                             settings.centerIndex >= input.controls.size() || settings.tetrahedra.empty() || settings.tetrahedra.size() > 4096)
                          : (input.controls.size() + MIN_RIG_CONTROL_POINTS > settings.maxControlPoints || !settings.tetrahedra.empty() ||
                             settings.bindingPositionScale != 1.f))) {
                return Invalid("Invalid tolerances or control budget; reserve the base cage and authored points within the renderer bone capacity");
            }
            std::set<std::string> controlIds, primitiveKeys, attachedKeys;
            for (const auto &control : input.controls) {
                if (control.id.empty() || control.id.starts_with("auto:") || control.id.find('\0') != std::string::npos || control.id.size() > 1024 ||
                    !controlIds.insert(control.id).second || !Finite(ToDouble(control.position)) ||
                    control.mode < phy::SoftBodyAttachmentMode::Dynamic || control.mode >= phy::SoftBodyAttachmentMode::END) {
                    return Invalid("Required controls need unique non-reserved IDs, finite positions and valid attachment modes");
                }
            }
            bodyIndex = input.source.primitives.size();
            for (size_t i = 0; i < input.source.primitives.size(); ++i) {
                const auto &primitive = input.source.primitives[i];
                if (primitive.key.empty() || !primitiveKeys.insert(primitive.key).second)
                    return Invalid("Duplicate or empty source primitive key");
                if (primitive.key == input.bodyPrimitive)
                    bodyIndex = i;
            }
            if (bodyIndex == input.source.primitives.size())
                return Invalid("The selected body primitive does not exist");
            for (const auto &attachment : input.attachments) {
                if (attachment.primitiveKey == input.bodyPrimitive || !primitiveKeys.contains(attachment.primitiveKey) ||
                    static_cast<uint32_t>(attachment.mode) >= static_cast<uint32_t>(AttachmentBindingMode::END) ||
                    (attachment.mode == AttachmentBindingMode::Control
                         ? !controlIds.contains(attachment.controlId)
                         : (!attachment.controlId.empty() || !Finite(ToDouble(attachment.bindingAnchor)))) ||
                    !attachedKeys.insert(attachment.primitiveKey).second) {
                    return Invalid("Every attachment must name a distinct non-body primitive and a required control ID");
                }
            }
            std::string missing;
            for (const auto &key : primitiveKeys)
                if (key != input.bodyPrimitive && !attachedKeys.contains(key))
                    missing += " " + key;
            if (!missing.empty())
                return {Error::Unsupported, "Non-body primitives require explicit rigid attachments:" + missing};
            const auto &primitive = input.source.primitives[bodyIndex];
            if (primitive.positions.size() < 4 || primitive.positions.size() > 1000000 || primitive.indices.empty() || primitive.indices.size() % 3 ||
                primitive.indices.size() > 3000000)
                return Invalid("Invalid or excessive body mesh size");
            for (const float value : primitive.transform.v)
                if (!std::isfinite(value))
                    return Invalid("Non-finite body transform");
            if (std::abs(primitive.transform[0][3]) > 1e-6f || std::abs(primitive.transform[1][3]) > 1e-6f ||
                std::abs(primitive.transform[2][3]) > 1e-6f || std::abs(primitive.transform[3][3] - 1.f) > 1e-6f) {
                return Invalid("Authoring requires an affine body transform");
            }
            surface.low  = {INFINITY, INFINITY, INFINITY};
            surface.high = {-INFINITY, -INFINITY, -INFINITY};
            std::map<std::array<float, 3>, uint32_t> welded;
            std::vector<uint32_t>                    weld;
            for (const auto &position : primitive.positions) {
                const auto baked = primitive.transform * Vector4(position.x, position.y, position.z, 1.f);
                const D3   point{baked.x, baked.y, baked.z};
                if (!Finite(point))
                    return Invalid("Non-finite body vertex");
                surface.points.push_back(point);
                surface.low  = Low(surface.low, point);
                surface.high = High(surface.high, point);
                const std::array<float, 3> key{baked.x, baked.y, baked.z};
                const auto [entry, inserted] = welded.emplace(key, static_cast<uint32_t>(welded.size()));
                (void)inserted;
                weld.push_back(entry->second);
            }
            surface.scale = Length(surface.high - surface.low);
            if (!(surface.scale > 1e-8))
                return Invalid("Body bounds are degenerate");
            struct EdgeUse {
                uint32_t count     = 0;
                int      direction = 0;
                uint32_t firstFace = 0;
            };
            std::map<Edge, EdgeUse>            edges;
            std::vector<std::vector<uint32_t>> adjacency(primitive.indices.size() / 3);
            for (size_t i = 0; i < primitive.indices.size(); i += 3) {
                Tri face{primitive.indices[i], primitive.indices[i + 1], primitive.indices[i + 2]};
                for (const uint32_t vertex : face)
                    if (vertex >= surface.points.size())
                        return Invalid("Body triangle index is out of bounds");
                const D3 a = surface.points[face[0]], b = surface.points[face[1]], c = surface.points[face[2]];
                if (Length(Cross(b - a, c - a)) <= surface.scale * surface.scale * 1e-12)
                    return Invalid("Body contains a degenerate triangle");
                surface.volume += Det(a - surface.low, b - surface.low, c - surface.low) / 6.;
                const uint32_t faceIndex = static_cast<uint32_t>(surface.faces.size());
                for (size_t k = 0; k < 3; ++k) {
                    const uint32_t x = weld[face[k]], y = weld[face[(k + 1) % 3]];
                    auto          &edge = edges[SortedEdge(x, y)];
                    if (edge.count) {
                        adjacency[faceIndex].push_back(edge.firstFace);
                        adjacency[edge.firstFace].push_back(faceIndex);
                    } else
                        edge.firstFace = faceIndex;
                    ++edge.count;
                    edge.direction += x < y ? 1 : -1;
                }
                surface.faces.push_back(face);
            }
            for (const auto &[edge, use] : edges) {
                (void)edge;
                if (use.count != 2 || use.direction != 0)
                    return Invalid("Body must be a closed, consistently wound manifold after exact-position seam welding");
            }
            std::vector<bool>     seen(surface.faces.size(), false);
            std::vector<uint32_t> pending{0};
            seen[0]        = true;
            size_t visited = 0;
            while (!pending.empty()) {
                const uint32_t face = pending.back();
                pending.pop_back();
                ++visited;
                for (const uint32_t neighbor : adjacency[face])
                    if (!seen[neighbor]) {
                        seen[neighbor] = true;
                        pending.push_back(neighbor);
                    }
            }
            if (visited != surface.faces.size())
                return Invalid("The body must be one connected closed shell");
            if (std::abs(surface.volume) <= surface.scale * surface.scale * surface.scale * 1e-12)
                return Invalid("Body volume is degenerate");
            if (surface.volume < 0.) {
                for (auto &face : surface.faces)
                    std::swap(face[1], face[2]);
                surface.volume = -surface.volume;
            }
            for (const auto &face : surface.faces)
                surface.normals.push_back(
                    Unit(Cross(surface.points[face[1]] - surface.points[face[0]], surface.points[face[2]] - surface.points[face[0]])));
            return {};
        }

        Status FindCenter(const RigGenerationInput &input, Surface &surface, const std::atomic_bool *cancel)
        {
            const double margin = surface.scale * 1e-8;
            surface.center      = input.settings.useExplicitCenter ? ToDouble(input.settings.cageCenter) : (surface.low + surface.high) * .5;
            if (!Finite(surface.center))
                return Invalid("Non-finite cage center");
            if (!input.settings.useExplicitCenter) {
                for (uint32_t sweep = 0; sweep < 256; ++sweep) {
                    if (Cancelled(cancel))
                        return CancelStatus();
                    double maximum = -INFINITY;
                    for (size_t i = 0; i < surface.faces.size(); ++i) {
                        const double distance = Dot(surface.normals[i], surface.center - surface.points[surface.faces[i][0]]) + margin * 2.;
                        maximum               = std::max(maximum, distance);
                        if (distance > 0.)
                            surface.center = surface.center - surface.normals[i] * distance;
                    }
                    if (maximum <= margin * .25)
                        break;
                }
            }
            for (size_t i = 0; i < surface.faces.size(); ++i) {
                if (Dot(surface.normals[i], surface.center - surface.points[surface.faces[i][0]]) >= -margin) {
                    return {Error::Unsupported,
                            "Body is not star-shaped about a verified interior center; choose a valid explicit center or another generator"};
                }
            }
            return {};
        }

        void MakeDirections(Points &directions, std::vector<Tri> &faces, uint32_t budget)
        {
            if (budget < 12) {
                directions = {{1., 0., 0.}, {-1., 0., 0.}, {0., 1., 0.}, {0., -1., 0.}, {0., 0., 1.}, {0., 0., -1.}};
                for (uint32_t x = 0; x < 2; ++x)
                    for (uint32_t y = 2; y < 4; ++y)
                        for (uint32_t z = 4; z < 6; ++z) {
                            Tri face{x, y, z};
                            if (Det(directions[x], directions[y], directions[z]) < 0.)
                                std::swap(face[1], face[2]);
                            faces.push_back(face);
                        }
                return;
            }
            const double phi = (1. + std::sqrt(5.)) * .5;
            for (const double a : {-1., 1.})
                for (const double b : {-1., 1.}) {
                    directions.push_back({0., a, b * phi});
                    directions.push_back({a, b * phi, 0.});
                    directions.push_back({b * phi, 0., a});
                }
            for (uint32_t a = 0; a < 12; ++a)
                for (uint32_t b = a + 1; b < 12; ++b)
                    for (uint32_t c = b + 1; c < 12; ++c) {
                        if (std::abs(Length(directions[a] - directions[b]) - 2.) < 1e-8 &&
                            std::abs(Length(directions[b] - directions[c]) - 2.) < 1e-8 &&
                            std::abs(Length(directions[c] - directions[a]) - 2.) < 1e-8) {
                            Tri face{a, b, c};
                            if (Det(directions[a], directions[b], directions[c]) < 0.)
                                std::swap(face[1], face[2]);
                            faces.push_back(face);
                        }
                    }
            for (auto &direction : directions)
                direction = Unit(direction);
            if (budget < 42)
                return;
            std::map<Edge, uint32_t> midpoints;
            const auto               midpoint = [&](uint32_t a, uint32_t b) {
                const auto key   = SortedEdge(a, b);
                const auto found = midpoints.find(key);
                if (found != midpoints.end())
                    return found->second;
                const uint32_t index = static_cast<uint32_t>(directions.size());
                directions.push_back(Unit(directions[a] + directions[b]));
                midpoints.emplace(key, index);
                return index;
            };
            std::vector<Tri> refined;
            for (const auto &face : faces) {
                const auto a = face[0], b = face[1], c = face[2];
                const auto ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
                refined.push_back({a, ab, ca});
                refined.push_back({ab, b, bc});
                refined.push_back({ca, bc, c});
                refined.push_back({ab, bc, ca});
            }
            faces = std::move(refined);
        }

        double RayRadius(const Surface &surface, D3 direction)
        {
            double radius = 0.;
            for (const auto &face : surface.faces) {
                const D3     a = surface.points[face[0]], e = surface.points[face[1]] - a, f = surface.points[face[2]] - a;
                const D3     p           = Cross(direction, f);
                const double determinant = Dot(e, p);
                if (std::abs(determinant) <= surface.scale * surface.scale * 1e-14)
                    continue;
                const D3     t = surface.center - a, q = Cross(t, e);
                const double u = Dot(t, p) / determinant, v = Dot(direction, q) / determinant, distance = Dot(f, q) / determinant;
                if (u >= -1e-9 && v >= -1e-9 && u + v <= 1. + 1e-9 && distance > 0.)
                    radius = std::max(radius, distance);
            }
            return radius;
        }

        Points ClipCone(Points polygon)
        {
            for (size_t axis = 0; axis < 3 && !polygon.empty(); ++axis) {
                Points next;
                next.reserve(8);
                for (size_t i = 0; i < polygon.size(); ++i) {
                    const D3   a = polygon[i], b = polygon[(i + 1) % polygon.size()];
                    const bool insideA = a[axis] >= 0., insideB = b[axis] >= 0.;
                    if (insideA)
                        next.push_back(a);
                    if (insideA != insideB)
                        next.push_back(a + (b - a) * (a[axis] / (a[axis] - b[axis])));
                }
                polygon = std::move(next);
            }
            for (auto &p : polygon)
                p = High(p, {});
            return polygon;
        }

        Status BuildCones(
            const Surface &surface, const Points &directions, const std::vector<Tri> &faces, std::vector<Cone> &cones, const std::atomic_bool *cancel)
        {
            cones.clear();
            cones.reserve(faces.size());
            for (const auto &face : faces) {
                if (Cancelled(cancel))
                    return CancelStatus();
                const D3     a = directions[face[0]], b = directions[face[1]], c = directions[face[2]];
                const double determinant = Det(a, b, c);
                if (determinant < 1e-10)
                    return {Error::CapacityExceeded, "Adaptive directions became numerically degenerate"};
                const D3 r0 = Cross(b, c) / determinant, r1 = Cross(c, a) / determinant, r2 = Cross(a, b) / determinant;
                Cone     cone;
                cone.face = face;
                Points transformed;
                transformed.reserve(surface.points.size());
                for (const auto &point : surface.points) {
                    const D3 p = point - surface.center;
                    transformed.push_back({Dot(r0, p), Dot(r1, p), Dot(r2, p)});
                }
                for (const auto &source : surface.faces) {
                    const D3 q0 = transformed[source[0]], q1 = transformed[source[1]], q2 = transformed[source[2]];
                    const D3 maximum = High(High(q0, q1), q2);
                    if (std::min({maximum.x, maximum.y, maximum.z}) < -surface.scale * 1e-11)
                        continue;
                    const auto clipped = ClipCone({q0, q1, q2});
                    cone.rows.insert(cone.rows.end(), clipped.begin(), clipped.end());
                }
                if (cone.rows.empty())
                    return {Error::InvalidState, "A radial cone has no source boundary"};
                cones.push_back(std::move(cone));
            }
            return {};
        }

        double ConeRatio(const Cone &cone, D3 row, const std::vector<double> &radii, double expansion)
        {
            return row.x / (radii[cone.face[0]] + expansion) + row.y / (radii[cone.face[1]] + expansion) + row.z / (radii[cone.face[2]] + expansion);
        }
        std::vector<double> FitRadii(const std::vector<Cone> &cones, const std::vector<double> &radial, double scale)
        {
            double low = 0., high = scale * 4.;
            for (uint32_t iteration = 0; iteration < 44; ++iteration) {
                const double middle  = (low + high) * .5;
                double       maximum = 0.;
                for (const auto &cone : cones)
                    for (const auto &row : cone.rows)
                        maximum = std::max(maximum, ConeRatio(cone, row, radial, middle));
                if (maximum > 1.)
                    low = middle;
                else
                    high = middle;
            }
            std::vector<double> inverse(radial.size());
            for (size_t i = 0; i < radial.size(); ++i)
                inverse[i] = 1. / (radial[i] + high + scale * 1e-9);
            std::vector<std::vector<std::pair<size_t, size_t>>> members(radial.size());
            for (size_t face = 0; face < cones.size(); ++face)
                for (size_t k = 0; k < 3; ++k)
                    members[cones[face].face[k]].push_back({face, k});
            for (uint32_t sweep = 0; sweep < 10; ++sweep) {
                double change = 0.;
                for (size_t node = 0; node < radial.size(); ++node) {
                    double upper = INFINITY;
                    for (const auto &[face, axis] : members[node]) {
                        const auto &cone = cones[face];
                        for (const auto &row : cone.rows) {
                            if (row[axis] <= scale * 1e-12)
                                continue;
                            double numerator = 1.;
                            for (size_t k = 0; k < 3; ++k)
                                if (k != axis)
                                    numerator -= row[k] * inverse[cone.face[k]];
                            upper = std::min(upper, numerator / row[axis]);
                        }
                    }
                    if (std::isfinite(upper) && upper > inverse[node]) {
                        change        = std::max(change, upper - inverse[node]);
                        inverse[node] = upper;
                    }
                }
                if (change < 1e-10 / scale)
                    break;
            }
            for (auto &value : inverse)
                value = 1.00001 / value;
            return inverse;
        }

        void SplitEdge(Points &directions, std::vector<Tri> &faces, Edge edge)
        {
            const uint32_t vertex = static_cast<uint32_t>(directions.size());
            directions.push_back(Unit(directions[edge[0]] + directions[edge[1]]));
            std::vector<Tri> next;
            for (const auto &face : faces) {
                if (!Has(face, edge[0]) || !Has(face, edge[1])) {
                    next.push_back(face);
                    continue;
                }
                uint32_t third = face[0];
                for (const auto index : face)
                    if (index != edge[0] && index != edge[1])
                        third = index;
                for (Tri split : {Tri{edge[0], vertex, third}, Tri{vertex, edge[1], third}}) {
                    if (Det(directions[split[0]], directions[split[1]], directions[split[2]]) < 0.)
                        std::swap(split[1], split[2]);
                    next.push_back(split);
                }
            }
            faces = std::move(next);
        }

        void Refine(const Surface             &surface,
                    Points                    &directions,
                    std::vector<Tri>          &faces,
                    const std::vector<Cone>   &cones,
                    const std::vector<double> &radial,
                    uint32_t                   budget)
        {
            std::vector<std::pair<double, size_t>> scores;
            for (size_t i = 0; i < cones.size(); ++i) {
                const auto &cone  = cones[i];
                double      score = 0.;
                D3          center{};
                for (size_t k = 0; k < 3; ++k)
                    center = center + directions[cone.face[k]] * (radial[cone.face[k]] / 3.);
                score = std::max(score, Length(center) - RayRadius(surface, Unit(center)));
                for (const auto &row : cone.rows) {
                    const double ratio = ConeRatio(cone, row, radial, 0.);
                    const D3     point = directions[cone.face[0]] * row.x + directions[cone.face[1]] * row.y + directions[cone.face[2]] * row.z;
                    if (ratio > 0.)
                        score = std::max(score, Length(point) * (1. - 1. / ratio));
                }
                scores.push_back({score, i});
            }
            std::sort(scores.begin(), scores.end(), std::greater<>());
            std::vector<Edge> selected;
            for (const auto &[score, index] : scores) {
                (void)score;
                const auto &face    = faces[index];
                Edge        longest = SortedEdge(face[0], face[1]);
                double      length  = Length(directions[longest[0]] - directions[longest[1]]);
                for (size_t k = 1; k < 3; ++k) {
                    const auto   edge      = SortedEdge(face[k], face[(k + 1) % 3]);
                    const double candidate = Length(directions[edge[0]] - directions[edge[1]]);
                    if (candidate > length) {
                        longest = edge;
                        length  = candidate;
                    }
                }
                if (std::find(selected.begin(), selected.end(), longest) != selected.end())
                    continue;
                selected.push_back(longest);
                if (selected.size() >= std::min<size_t>(10, budget - directions.size()))
                    break;
            }
            for (const auto &edge : selected)
                SplitEdge(directions, faces, edge);
        }

        Status BuildStarVolume(const Surface &surface, uint32_t budget, Points &nodes, std::vector<Tet> &tets, const std::atomic_bool *cancel)
        {
            Points           directions;
            std::vector<Tri> faces;
            MakeDirections(directions, faces, budget);
            std::vector<Cone>   cones;
            std::vector<double> radial, radii;
            while (true) {
                if (Cancelled(cancel))
                    return CancelStatus();
                radial.clear();
                for (const auto &direction : directions) {
                    const double radius = RayRadius(surface, direction);
                    if (!(radius > surface.scale * 1e-8))
                        return {Error::Unsupported, "The body has a missing or degenerate radial intersection"};
                    radial.push_back(radius);
                }
                if (const auto status = BuildCones(surface, directions, faces, cones, cancel); !status)
                    return status;
                if (directions.size() >= budget) {
                    radii = FitRadii(cones, radial, surface.scale);
                    break;
                }
                Refine(surface, directions, faces, cones, radial, budget);
            }
            nodes = {ToDouble(ToFloat(surface.center))};
            for (size_t i = 0; i < directions.size(); ++i)
                nodes.push_back(ToDouble(ToFloat(surface.center + directions[i] * radii[i])));
            for (const auto &face : faces) {
                Tet tet{0, face[0] + 1, face[1] + 1, face[2] + 1};
                if (Volume6(nodes, tet) < 0.)
                    std::swap(tet[1], tet[2]);
                tets.push_back(tet);
            }
            return {};
        }

        Status InsertControls(const RigGenerationInput                 &input,
                              Points                                   &nodes,
                              std::vector<Tet>                         &tets,
                              std::vector<std::string>                 &ids,
                              std::vector<phy::SoftBodyAttachmentMode> &modes,
                              double                                    scale)
        {
            for (const auto &control : input.controls) {
                const D3 point    = ToDouble(control.position);
                uint32_t existing = UINT32_MAX;
                for (uint32_t i = 0; i < nodes.size(); ++i)
                    if (Length(nodes[i] - point) <= scale * 1e-10) {
                        existing = i;
                        break;
                    }
                std::vector<uint32_t> support;
                if (existing == UINT32_MAX) {
                    for (const auto &tet : tets) {
                        const auto weights = Barycentric(nodes, tet, point);
                        if (*std::min_element(weights.begin(), weights.end()) < -1e-10)
                            continue;
                        for (size_t k = 0; k < 4; ++k)
                            if (weights[k] > 1e-10)
                                support.push_back(tet[k]);
                        break;
                    }
                    if (support.empty())
                        return Invalid("Required control is outside the generated volume: " + control.id);
                    if (support.size() == 1)
                        existing = support[0];
                }
                if (existing != UINT32_MAX) {
                    if (!ids[existing].starts_with("auto:"))
                        return Invalid("Required controls cannot share a position");
                    nodes[existing] = point;
                    ids[existing]   = control.id;
                    modes[existing] = control.mode;
                    continue;
                }
                if (nodes.size() >= input.settings.maxControlPoints)
                    return {Error::CapacityExceeded, "Required controls exceed the remaining bone budget"};
                const uint32_t vertex = static_cast<uint32_t>(nodes.size());
                nodes.push_back(point);
                ids.push_back(control.id);
                modes.push_back(control.mode);
                std::vector<Tet> next;
                for (const auto &tet : tets) {
                    bool contains = true;
                    for (const auto index : support)
                        contains &= std::find(tet.begin(), tet.end(), index) != tet.end();
                    if (!contains) {
                        next.push_back(tet);
                        continue;
                    }
                    for (const auto replaced : support) {
                        Tet split = tet;
                        for (auto &index : split)
                            if (index == replaced)
                                index = vertex;
                        if (Volume6(nodes, split) < 0.)
                            std::swap(split[1], split[2]);
                        next.push_back(split);
                    }
                }
                tets = std::move(next);
            }
            return {};
        }

        double TetQuality(const Points &nodes, const Tet &tet)
        {
            double longest = 0.;
            for (size_t i = 0; i < 4; ++i)
                for (size_t j = i + 1; j < 4; ++j)
                    longest = std::max(longest, Length(nodes[tet[i]] - nodes[tet[j]]));
            return std::sqrt(2.) * Volume6(nodes, tet) / (longest * longest * longest);
        }

        Status FillControlBudget(const RigGeneratorSettings               &settings,
                                 Points                                   &nodes,
                                 std::vector<Tet>                         &tets,
                                 std::vector<std::string>                 &ids,
                                 std::vector<phy::SoftBodyAttachmentMode> &modes,
                                 double                                    scale,
                                 const std::atomic_bool                   *cancel)
        {
            if (!settings.fillControlBudget)
                return {};
            if (nodes.size() > settings.maxControlPoints)
                return {Error::CapacityExceeded, "Generated cage exceeds the exact control budget"};
            while (nodes.size() < settings.maxControlPoints) {
                if (Cancelled(cancel))
                    return CancelStatus();
                std::map<Tri, uint32_t> faceUses;
                for (const auto &tet : tets)
                    for (size_t omitted = 0; omitted < 4; ++omitted) {
                        Tri    face{};
                        size_t cursor = 0;
                        for (size_t k = 0; k < 4; ++k)
                            if (k != omitted)
                                face[cursor++] = tet[k];
                        std::sort(face.begin(), face.end());
                        ++faceUses[face];
                    }
                std::set<Edge> boundaryEdges;
                for (const auto &[face, uses] : faceUses)
                    if (uses == 1)
                        for (size_t k = 0; k < 3; ++k)
                            boundaryEdges.insert(SortedEdge(face[k], face[(k + 1) % 3]));
                std::vector<std::pair<double, Edge>> edges;
                for (const auto &edge : boundaryEdges)
                    edges.push_back({Length(nodes[edge[0]] - nodes[edge[1]]), edge});
                std::sort(edges.begin(), edges.end(), std::greater<>());
                bool refined = false;
                for (const auto &[length, edge] : edges) {
                    if (Cancelled(cancel))
                        return CancelStatus();
                    const auto midpoint = ToDouble(ToFloat((nodes[edge[0]] + nodes[edge[1]]) * .5));
                    if (!Finite(midpoint) || length <= scale * 1e-10 ||
                        std::any_of(nodes.begin(), nodes.end(), [&](D3 point) { return Length(point - midpoint) <= scale * 1e-10; }))
                        continue;
                    const auto vertex = static_cast<uint32_t>(nodes.size());
                    nodes.push_back(midpoint);
                    std::vector<Tet> next;
                    bool             valid = true;
                    for (const auto &tet : tets) {
                        if (std::find(tet.begin(), tet.end(), edge[0]) == tet.end() || std::find(tet.begin(), tet.end(), edge[1]) == tet.end()) {
                            next.push_back(tet);
                            continue;
                        }
                        // Every incident tetrahedron shares the stored-float midpoint, preserving conformity.
                        for (const auto replaced : edge) {
                            auto split = tet;
                            for (auto &index : split)
                                if (index == replaced)
                                    index = vertex;
                            const double quality = TetQuality(nodes, split);
                            if (!std::isfinite(quality) || quality < settings.minTetQuality) {
                                valid = false;
                                break;
                            }
                            next.push_back(split);
                        }
                        if (!valid)
                            break;
                    }
                    if (!valid) {
                        nodes.pop_back();
                        continue;
                    }
                    tets = std::move(next);
                    ids.push_back("auto:" + std::to_string(vertex));
                    modes.push_back(phy::SoftBodyAttachmentMode::Dynamic);
                    refined = true;
                    break;
                }
                if (!refined)
                    return {Error::CapacityExceeded, "Exact control budget cannot be filled without violating tetrahedron quality"};
            }
            return {};
        }

        Status ValidateTets(const Points               &nodes,
                            const std::vector<Tet>     &tets,
                            const RigGeneratorSettings &settings,
                            RigGenerationDiagnostics   &diagnostics,
                            std::vector<Tri>           &boundary)
        {
            struct FaceUse {
                Tri      face{};
                uint32_t count = 0;
            };
            std::map<Tri, FaceUse> allFaces;
            std::set<Tet>          uniqueTets;
            diagnostics.minimumTetQuality = INFINITY;
            diagnostics.cageVolume        = 0.;
            for (const auto &tet : tets) {
                auto sorted = tet;
                std::sort(sorted.begin(), sorted.end());
                if (sorted.back() >= nodes.size() || std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end() ||
                    !uniqueTets.insert(sorted).second)
                    return Invalid("Invalid or duplicate cage tetrahedron");
                const double volume6 = Volume6(nodes, tet);
                const double quality = TetQuality(nodes, tet);
                if (!std::isfinite(volume6) || volume6 <= 1e-12 || !std::isfinite(quality) || quality < settings.minTetQuality)
                    return {Error::CapacityExceeded,
                            "A tetrahedron violates the configured quality threshold; adjust required points or the control budget"};
                diagnostics.minimumTetQuality = std::min(diagnostics.minimumTetQuality, quality);
                diagnostics.cageVolume += volume6 / 6.;
                for (size_t omitted = 0; omitted < 4; ++omitted) {
                    Tri    face{};
                    size_t cursor = 0;
                    for (size_t k = 0; k < 4; ++k)
                        if (k != omitted)
                            face[cursor++] = tet[k];
                    if (Dot(Cross(nodes[face[1]] - nodes[face[0]], nodes[face[2]] - nodes[face[0]]), nodes[tet[omitted]] - nodes[face[0]]) > 0.)
                        std::swap(face[1], face[2]);
                    Tri key = face;
                    std::sort(key.begin(), key.end());
                    auto &use = allFaces[key];
                    if (use.count == 1) {
                        bool opposite = false;
                        for (size_t k = 0; k < 3; ++k)
                            opposite |= face[k] == use.face[0] && face[(k + 1) % 3] == use.face[2] && face[(k + 2) % 3] == use.face[1];
                        if (!opposite)
                            return Invalid("Adjacent cage tetrahedra overlap across a shared face");
                    }
                    use.face = face;
                    ++use.count;
                    if (use.count > 2)
                        return {Error::InvalidState, "Generated volume contains a non-manifold face"};
                }
            }
            boundary.clear();
            std::map<Edge, uint32_t> edges;
            for (const auto &[key, use] : allFaces) {
                (void)key;
                if (use.count != 1)
                    continue;
                boundary.push_back(use.face);
                for (size_t k = 0; k < 3; ++k)
                    ++edges[SortedEdge(use.face[k], use.face[(k + 1) % 3])];
            }
            for (const auto &[edge, count] : edges) {
                (void)edge;
                if (count != 2)
                    return {Error::InvalidState, "Generated boundary is not a closed two-manifold"};
            }
            diagnostics.relativeVolumeExcess = diagnostics.cageVolume / diagnostics.sourceVolume - 1.;
            if ((settings.mode == RigGenerationMode::AutoFit && diagnostics.relativeVolumeExcess < -1e-6) ||
                diagnostics.relativeVolumeExcess > settings.maxVolumeExcess) {
                return {Error::CapacityExceeded, "Cage volume exceeds the configured volume error budget"};
            }
            return {};
        }

        Status VerifyCoverage(Surface                   surface,
                              const Points             &nodes,
                              const std::vector<Tri>   &boundary,
                              RigGenerationDiagnostics &diagnostics,
                              const std::atomic_bool   *cancel)
        {
            surface.center = nodes[0];
            Points              directions;
            std::vector<double> radii;
            for (const auto &node : nodes) {
                const D3     delta  = node - surface.center;
                const double radius = Length(delta);
                directions.push_back(radius > 0. ? delta / radius : D3{});
                radii.push_back(radius);
            }
            std::vector<Cone> cones;
            if (const auto status = BuildCones(surface, directions, boundary, cones, cancel); !status)
                return status;
            for (const auto &cone : cones)
                for (const auto &row : cone.rows) {
                    if (ConeRatio(cone, row, radii, 0.) > 1. + 1e-8) {
                        ++diagnostics.uncoveredTriangleCount;
                        return {Error::CapacityExceeded, "Float conversion or required control insertion invalidated complete triangle coverage"};
                    }
                }
            return {};
        }

        Status VerifySuppliedFan(const RigGenerationInput &input,
                                 const Surface            &source,
                                 const Points             &nodes,
                                 const std::vector<Tet>   &tets,
                                 const std::vector<Tri>   &boundary,
                                 const std::atomic_bool   *cancel)
        {
            const uint32_t center = input.settings.centerIndex;
            if (boundary.size() != tets.size())
                return Invalid("A supplied center fan requires one boundary face per tetrahedron");
            for (const auto &tet : tets)
                if (std::find(tet.begin(), tet.end(), center) == tet.end())
                    return Invalid("Every supplied tetrahedron must contain the declared center");
            std::set<uint32_t>                                    shell;
            std::map<Edge, std::vector<std::pair<uint32_t, int>>> edges;
            std::vector<std::vector<uint32_t>>                    neighbors(boundary.size());
            double                                                solidAngle = 0.;
            const double                                          margin     = source.scale * 1e-8;
            for (uint32_t faceIndex = 0; faceIndex < boundary.size(); ++faceIndex) {
                if (Cancelled(cancel))
                    return CancelStatus();
                const auto &face = boundary[faceIndex];
                if (Has(face, center))
                    return Invalid("Supplied fan center must not lie on the boundary");
                const D3 a = nodes[face[0]], b = nodes[face[1]], c = nodes[face[2]];
                const D3 normal = Unit(Cross(b - a, c - a));
                if (!Finite(normal) || Dot(normal, nodes[center] - a) >= -margin)
                    return Invalid("Supplied fan center is not strictly inside its boundary");
                for (const auto &node : nodes)
                    if (Dot(normal, node - a) > margin)
                        return {Error::Unsupported, "Supplied center fan requires a convex boundary"};
                // A convex cage containing all embedded corners contains each complete embedded triangle.
                for (const auto &point : source.points)
                    if (Dot(normal, point * input.settings.bindingPositionScale - a) > margin)
                        return Invalid("An embedded display triangle escapes the supplied cage");
                for (size_t k = 0; k < 3; ++k) {
                    const uint32_t x = face[k], y = face[(k + 1) % 3];
                    shell.insert(x);
                    edges[SortedEdge(x, y)].push_back({faceIndex, x < y ? 1 : -1});
                }
                const D3     da = a - nodes[center], db = b - nodes[center], dc = c - nodes[center];
                const double la = Length(da), lb = Length(db), lc = Length(dc);
                solidAngle += 2. * std::atan2(Det(da, db, dc), la * lb * lc + Dot(da, db) * lc + Dot(db, dc) * la + Dot(dc, da) * lb);
            }
            if (shell.size() + 1 != nodes.size() || shell.size() + boundary.size() != edges.size() + 2 ||
                std::abs(solidAngle - 4. * std::acos(-1.)) > 1e-5)
                return Invalid("Supplied center fan must use every control in one closed degree-one shell");
            for (const auto &[edge, uses] : edges) {
                (void)edge;
                if (uses.size() != 2 || uses[0].second + uses[1].second != 0)
                    return Invalid("Supplied boundary is not consistently oriented and closed");
                neighbors[uses[0].first].push_back(uses[1].first);
                neighbors[uses[1].first].push_back(uses[0].first);
            }
            std::vector<bool>     seen(boundary.size(), false);
            std::vector<uint32_t> pending{0};
            seen[0]      = true;
            size_t count = 0;
            while (!pending.empty()) {
                const uint32_t face = pending.back();
                pending.pop_back();
                ++count;
                for (const uint32_t neighbor : neighbors[face])
                    if (!seen[neighbor]) {
                        seen[neighbor] = true;
                        pending.push_back(neighbor);
                    }
            }
            return count == boundary.size() ? Status{} : Invalid("Supplied boundary contains disconnected shells");
        }

        Status CertifySurface(const Surface            &source,
                              const Points             &nodes,
                              const std::vector<Tri>   &boundary,
                              double                    tolerance,
                              RigGenerationDiagnostics &diagnostics,
                              const std::atomic_bool   *cancel)
        {
            const TriangleBvh bvh(source.points, source.faces);
            const double      accuracy = std::min(tolerance * .05, source.scale * .002);
            struct Patch {
                std::array<D3, 3> points;
                uint32_t          depth;
            };
            std::vector<Patch> pending;
            D3                 low{INFINITY, INFINITY, INFINITY}, high{-INFINITY, -INFINITY, -INFINITY};
            for (const auto &face : boundary) {
                pending.push_back({{nodes[face[0]], nodes[face[1]], nodes[face[2]]}, 0});
                for (const auto index : face) {
                    low                                       = Low(low, nodes[index]);
                    high                                      = High(high, nodes[index]);
                    diagnostics.maximumSampledSurfaceDistance = std::max(diagnostics.maximumSampledSurfaceDistance, bvh.Distance(nodes[index]));
                }
            }
            diagnostics.axisSupportGaps = {source.low.x - low.x,   high.x - source.high.x, source.low.y - low.y,
                                           high.y - source.high.y, source.low.z - low.z,   high.z - source.high.z};
            if (diagnostics.maximumSampledSurfaceDistance > tolerance)
                return {Error::CapacityExceeded, "A cage vertex exceeds the configured distance from the display mesh"};
            uint32_t queries = 0;
            while (!pending.empty()) {
                if (Cancelled(cancel))
                    return CancelStatus();
                const auto patch = pending.back();
                pending.pop_back();
                const D3     a = patch.points[0], b = patch.points[1], c = patch.points[2], center = (a + b + c) / 3.;
                const double distance                     = bvh.Distance(center) + source.scale * 1e-10;
                const double radius                       = std::max({Length(a - center), Length(b - center), Length(c - center)});
                diagnostics.maximumSampledSurfaceDistance = std::max(diagnostics.maximumSampledSurfaceDistance, distance);
                if (distance > tolerance)
                    return {Error::CapacityExceeded, "Cage surface exceeds the configured distance from the display mesh"};
                if (distance + radius <= tolerance && distance + radius <= diagnostics.maximumSampledSurfaceDistance + accuracy) {
                    diagnostics.maximumSurfaceDistance = std::max(diagnostics.maximumSurfaceDistance, distance + radius);
                    continue;
                }
                if (patch.depth >= 12 || ++queries > 1000000)
                    return {Error::CapacityExceeded, "Unable to certify surface distance within the bounded refinement budget"};
                const D3       ab = (a + b) * .5, bc = (b + c) * .5, ca = (c + a) * .5;
                const uint32_t depth = patch.depth + 1;
                pending.push_back({{a, ab, ca}, depth});
                pending.push_back({{ab, b, bc}, depth});
                pending.push_back({{ca, bc, c}, depth});
                pending.push_back({{ab, bc, ca}, depth});
            }
            return {};
        }

        Status MakeBinding(const RigGenerationInput &input,
                           size_t                    bodyIndex,
                           const Surface            &surface,
                           const Points             &nodes,
                           const std::vector<Tet>   &tets,
                           RigGenerationOutput      &output,
                           const std::atomic_bool   *cancel)
        {
            output.binding.bones.push_back({"root", UINT16_MAX, {}});
            for (size_t i = 0; i < nodes.size(); ++i) {
                phy::AuthoringBone bone;
                bone.name                  = "root/" + output.controlIds[i];
                bone.parent                = 0;
                bone.reference.translation = ToFloat(nodes[i]);
                output.binding.bones.push_back(std::move(bone));
            }
            uint64_t   signature = 14695981039346656037ULL;
            const auto hashByte  = [&](uint8_t value) { signature = (signature ^ value) * 1099511628211ULL; };
            for (const auto &bone : output.binding.bones) {
                for (const unsigned char value : bone.name)
                    hashByte(value);
                hashByte(0);
                for (size_t axis = 0; axis < 3; ++axis) {
                    const auto bits = std::bit_cast<uint32_t>(bone.reference.translation[axis]);
                    for (uint32_t shift = 0; shift < 32; shift += 8)
                        hashByte(static_cast<uint8_t>(bits >> shift));
                }
            }
            output.binding.layoutSignature = signature == 0 ? 1 : signature;
            const auto bind                = [&](D3 point, phy::AuthoringSkinWeights &entry) -> Status {
                const D3 embedded = point * input.settings.bindingPositionScale;
                for (const auto &tet : tets) {
                    auto weights = Barycentric(nodes, tet, embedded);
                    if (!std::all_of(weights.begin(), weights.end(), [](double w) { return std::isfinite(w) && w >= -1e-8; }))
                        continue;
                    double sum = 0.;
                    for (auto &weight : weights) {
                        weight = std::max(0., weight);
                        sum += weight;
                    }
                    size_t largest  = 0;
                    float  floatSum = 0.f;
                    for (size_t k = 0; k < 4; ++k) {
                        entry.bones[k]   = static_cast<uint16_t>(tet[k] + 1);
                        entry.weights[k] = static_cast<float>(weights[k] / sum);
                        floatSum += entry.weights[k];
                        if (entry.weights[k] > entry.weights[largest])
                            largest = k;
                    }
                    entry.weights[largest] += 1.f - floatSum;
                    D3 reconstructed{}, rest{};
                    for (size_t k = 0; k < 4; ++k) {
                        reconstructed = reconstructed + nodes[tet[k]] * entry.weights[k];
                        rest          = rest + point * entry.weights[k];
                    }
                    const double error = Length(reconstructed - embedded), restError = Length(rest - point);
                    if (error > surface.scale * 1e-5 || restError > surface.scale * 1e-5)
                        return {Error::InvalidState, "Skin weights exceed the binding or rest reconstruction tolerance"};
                    output.diagnostics.maximumBindingPositionError    = std::max(output.diagnostics.maximumBindingPositionError, error);
                    output.diagnostics.maximumRestReconstructionError = std::max(
                        output.diagnostics.maximumRestReconstructionError, input.settings.mode == RigGenerationMode::AutoFit ? error : restError);
                    return {};
                }
                return {Error::InvalidState, "A binding position is not inside the certified tetrahedral cage"};
            };
            for (size_t primitiveIndex = 0; primitiveIndex < input.source.primitives.size(); ++primitiveIndex) {
                const auto                    &primitive = input.source.primitives[primitiveIndex];
                phy::AuthoringPrimitiveBinding binding;
                binding.key = primitive.key;
                binding.vertices.reserve(primitive.positions.size());
                if (primitiveIndex != bodyIndex) {
                    const auto                attachment = std::find_if(input.attachments.begin(), input.attachments.end(),
                                                                        [&](const auto &entry) { return entry.primitiveKey == primitive.key; });
                    phy::AuthoringSkinWeights weights;
                    if (attachment->mode == AttachmentBindingMode::BindingAnchor) {
                        if (const auto status = bind(ToDouble(attachment->bindingAnchor), weights); !status)
                            return status;
                    } else {
                        const auto     control = std::find(output.controlIds.begin(), output.controlIds.end(), attachment->controlId);
                        const uint16_t bone    = static_cast<uint16_t>(std::distance(output.controlIds.begin(), control) + 1);
                        weights                = {{bone, 0, 0, 0}, {1.f, 0.f, 0.f, 0.f}};
                    }
                    binding.vertices.resize(primitive.positions.size(), weights);
                } else {
                    for (const auto &point : surface.points) {
                        if (Cancelled(cancel))
                            return CancelStatus();
                        phy::AuthoringSkinWeights entry;
                        if (const auto status = bind(point, entry); !status)
                            return status;
                        binding.vertices.push_back(entry);
                    }
                }
                output.binding.primitives.push_back(std::move(binding));
            }
            return {};
        }
        Status CompleteRig(const RigGenerationInput &input,
                           size_t                    bodyIndex,
                           const Surface            &surface,
                           Points                    nodes,
                           std::vector<Tet>          tets,
                           const char               *algorithm,
                           RigGenerationOutput      &out,
                           const std::atomic_bool   *cancel)
        {
            RigGenerationOutput candidate;
            candidate.diagnostics.algorithm              = algorithm;
            candidate.diagnostics.sourceVertexCount      = static_cast<uint32_t>(surface.points.size());
            candidate.diagnostics.sourceTriangleCount    = static_cast<uint32_t>(surface.faces.size());
            candidate.diagnostics.sourceVolume           = surface.volume;
            candidate.diagnostics.bindingPositionScale   = input.settings.bindingPositionScale;
            candidate.diagnostics.coverageInBindingSpace = input.settings.mode == RigGenerationMode::SuppliedCenterFan;
            std::vector<phy::SoftBodyAttachmentMode> modes(nodes.size(), phy::SoftBodyAttachmentMode::Dynamic);
            if (input.settings.mode == RigGenerationMode::SuppliedCenterFan) {
                for (size_t i = 0; i < nodes.size(); ++i) {
                    candidate.controlIds.push_back(input.controls[i].id);
                    modes[i] = input.controls[i].mode;
                }
            } else {
                for (size_t i = 0; i < nodes.size(); ++i)
                    candidate.controlIds.push_back("auto:" + std::to_string(i));
                if (const auto status = InsertControls(input, nodes, tets, candidate.controlIds, modes, surface.scale); !status)
                    return status;
                if (const auto status = FillControlBudget(input.settings, nodes, tets, candidate.controlIds, modes, surface.scale, cancel); !status)
                    return status;
            }
            std::vector<Tri> boundary;
            if (const auto status = ValidateTets(nodes, tets, input.settings, candidate.diagnostics, boundary); !status)
                return status;
            const auto coverage = input.settings.mode == RigGenerationMode::SuppliedCenterFan
                                      ? VerifySuppliedFan(input, surface, nodes, tets, boundary, cancel)
                                      : VerifyCoverage(surface, nodes, boundary, candidate.diagnostics, cancel);
            if (!coverage)
                return coverage;
            if (const auto status = CertifySurface(surface, nodes, boundary, input.settings.maxSurfaceDistance, candidate.diagnostics, cancel);
                !status)
                return status;
            if (const auto status = MakeBinding(input, bodyIndex, surface, nodes, tets, candidate, cancel); !status)
                return status;
            std::set<Edge> edges;
            for (const auto &tet : tets) {
                candidate.mesh.tetrahedra.push_back({tet, 0.f});
                for (size_t i = 0; i < 4; ++i)
                    for (size_t j = i + 1; j < 4; ++j)
                        edges.insert(SortedEdge(tet[i], tet[j]));
            }
            for (const auto &edge : edges)
                candidate.mesh.edges.push_back({edge, 0.f});
            for (size_t i = 0; i < nodes.size(); ++i)
                candidate.mesh.vertices.push_back(
                    {ToFloat(nodes[i]), VEC3_ZERO, modes[i] == phy::SoftBodyAttachmentMode::Dynamic ? 1.f : 0.f, modes[i]});
            candidate.mesh.faces                    = std::move(boundary);
            candidate.diagnostics.controlPointCount = static_cast<uint32_t>(nodes.size());
            candidate.diagnostics.tetrahedronCount  = static_cast<uint32_t>(tets.size());
            phy::SoftBodyDesc validation;
            validation.mesh = candidate.mesh;
            if (const auto status = phy::ValidateSoftBodyDesc(validation); !status)
                return status;
            if (Cancelled(cancel))
                return CancelStatus();
            out = std::move(candidate);
            return {};
        }
    } // namespace

    phy::PhysicsStatus GenerateRig(const RigGenerationInput &input, RigGenerationOutput &out, const std::atomic_bool *cancel)
    {
        if (Cancelled(cancel))
            return CancelStatus();
        Surface surface;
        size_t  bodyIndex = 0;
        if (const auto status = LoadSurface(input, surface, bodyIndex); !status)
            return status;
        if (input.settings.mode == RigGenerationMode::SuppliedCenterFan) {
            Points           nodes;
            std::vector<Tet> tets;
            for (const auto &control : input.controls)
                nodes.push_back(ToDouble(control.position));
            for (const auto &tet : input.settings.tetrahedra)
                tets.push_back({tet.a, tet.b, tet.c, tet.d});
            return CompleteRig(input, bodyIndex, surface, std::move(nodes), std::move(tets), "supplied-center-fan-v1", out, cancel);
        }
        if (const auto status = FindCenter(input, surface, cancel); !status)
            return status;
        double solidAngle = 0.;
        for (const auto &face : surface.faces) {
            const D3 a = surface.points[face[0]] - surface.center, b = surface.points[face[1]] - surface.center,
                     c      = surface.points[face[2]] - surface.center;
            const double la = Length(a), lb = Length(b), lc = Length(c);
            solidAngle += 2. * std::atan2(Det(a, b, c), la * lb * lc + Dot(a, b) * lc + Dot(b, c) * la + Dot(c, a) * lb);
        }
        if (std::abs(solidAngle - 4. * std::acos(-1.)) > 1e-5)
            return {Error::Unsupported, "The body does not form a single radial shell around the selected center"};
        const uint32_t      shellBudget = input.settings.maxControlPoints - 1 - static_cast<uint32_t>(input.controls.size());
        Points              nodes;
        std::vector<Tet>    tets;
        RigGenerationOutput star, support;
        auto                starStatus = BuildStarVolume(surface, shellBudget, nodes, tets, cancel);
        if (starStatus)
            starStatus = CompleteRig(input, bodyIndex, surface, std::move(nodes), std::move(tets),
                                     shellBudget < 12 ? "adaptive-octa-cage-v1" : "adaptive-star-cage-v1", star, cancel);
        if (Cancelled(cancel))
            return CancelStatus();
        SupportCage cage;
        auto        supportStatus = shellBudget >= 8 ? BuildSupportCage(surface.points, surface.center, shellBudget, cancel, cage)
                                                     : Status{Error::Unsupported, "Support cage requires eight boundary controls"};
        if (supportStatus) {
            nodes = {ToDouble(ToFloat(surface.center))};
            for (const auto &point : cage.nodes)
                nodes.push_back(ToDouble(ToFloat(point)));
            tets.clear();
            for (const auto &face : cage.faces)
                tets.push_back({0, face[0] + 1, face[1] + 1, face[2] + 1});
            supportStatus = CompleteRig(input, bodyIndex, surface, std::move(nodes), std::move(tets), "adaptive-support-cage-v2", support, cancel);
        }
        if (Cancelled(cancel))
            return CancelStatus();
        // Competing profiles pass identical geometry and binding checks before either can be published.
        if (supportStatus && (!starStatus || support.diagnostics.cageVolume < star.diagnostics.cageVolume)) {
            out = std::move(support);
            return {};
        }
        if (starStatus) {
            out = std::move(star);
            return {};
        }
        return {starStatus.error,
                "No cage profile met the requested constraints. Star: " + starStatus.message + "; support: " + supportStatus.message};
    }
} // namespace sky::softbody
