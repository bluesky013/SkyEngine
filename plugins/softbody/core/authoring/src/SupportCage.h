#pragma once

#include "Geometry.h"

#include <physics/PhysicsStatus.h>

#include <atomic>
#include <cstddef>
#include <map>
#include <set>
#include <utility>

namespace sky::softbody::detail {
    struct SupportCage {
        // Boundary vertices only; the caller owns center and authored control insertion.
        Points           nodes;
        std::vector<Tri> faces;
    };

    namespace support_cage {
        using Edge    = std::array<uint32_t, 2>;
        using Polygon = std::vector<uint32_t>;
        struct Polyhedron {
            Points               points;
            std::vector<Polygon> faces;
        };
        struct Plane {
            D3     normal;
            double support = 0.;
        };
        inline bool Cancelled(const std::atomic_bool *cancel)
        {
            return cancel && cancel->load(std::memory_order_relaxed);
        }
        inline Edge EdgeKey(uint32_t a, uint32_t b)
        {
            return a < b ? Edge{a, b} : Edge{b, a};
        }
        inline D3 Area(const Points &points, const Polygon &face)
        {
            D3 result{};
            for (size_t i = 1; i + 1 < face.size(); ++i)
                result = result + Cross(points[face[i]] - points[face[0]], points[face[i + 1]] - points[face[0]]);
            return result;
        }
        inline double Volume(const Polyhedron &poly, D3 center)
        {
            double result = 0.;
            for (const auto &face : poly.faces)
                for (size_t i = 1; i + 1 < face.size(); ++i)
                    result += Det(poly.points[face[0]] - center, poly.points[face[i]] - center, poly.points[face[i + 1]] - center) / 6.;
            return result;
        }
        inline Points Directions()
        {
            const double phi = (1. + std::sqrt(5.)) * .5;
            Points       points;
            for (const double a : {-1., 1.})
                for (const double b : {-1., 1.}) {
                    points.push_back({0., a, b * phi});
                    points.push_back({a, b * phi, 0.});
                    points.push_back({b * phi, 0., a});
                }
            std::vector<Tri> faces;
            for (uint32_t a = 0; a < 12; ++a)
                for (uint32_t b = a + 1; b < 12; ++b)
                    for (uint32_t c = b + 1; c < 12; ++c)
                        if (std::abs(Length(points[a] - points[b]) - 2.) < 1e-8 && std::abs(Length(points[b] - points[c]) - 2.) < 1e-8 &&
                            std::abs(Length(points[c] - points[a]) - 2.) < 1e-8) {
                            Tri face{a, b, c};
                            if (Det(points[a], points[b], points[c]) < 0.)
                                std::swap(face[1], face[2]);
                            faces.push_back(face);
                        }
            for (auto &point : points)
                point = Unit(point);
            for (uint32_t level = 0; level < 3; ++level) {
                std::map<Edge, uint32_t> midpoints;
                const auto               midpoint = [&](uint32_t a, uint32_t b) {
                    const auto key = EdgeKey(a, b);
                    if (const auto found = midpoints.find(key); found != midpoints.end())
                        return found->second;
                    const auto index = static_cast<uint32_t>(points.size());
                    points.push_back(Unit(points[a] + points[b]));
                    midpoints.emplace(key, index);
                    return index;
                };
                std::vector<Tri> refined;
                for (const auto &face : faces) {
                    const auto a = face[0], b = face[1], c = face[2];
                    const auto ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
                    for (const Tri split : {Tri{a, ab, ca}, Tri{ab, b, bc}, Tri{ca, bc, c}, Tri{ab, bc, ca}})
                        refined.push_back(split);
                }
                faces = std::move(refined);
            }
            return points;
        }
        inline bool Clip(const Polyhedron &input, const Plane &plane, uint32_t budget, Polyhedron &out)
        {
            std::vector<double> distances;
            bool                inside = false, outside = false;
            for (const auto point : input.points) {
                const double distance = Dot(plane.normal, point) - plane.support;
                distances.push_back(distance);
                inside |= distance <= 0.;
                outside |= distance > 0.;
            }
            if (!inside || !outside)
                return false;
            Polyhedron candidate;
            candidate.points = input.points;
            std::map<Edge, uint32_t> intersections;
            std::set<uint32_t>       capVertices;
            const auto               intersection = [&](uint32_t a, uint32_t b) {
                if (distances[a] == 0.)
                    return a;
                if (distances[b] == 0.)
                    return b;
                const auto key = EdgeKey(a, b);
                if (const auto found = intersections.find(key); found != intersections.end())
                    return found->second;
                const double fraction = distances[key[0]] / (distances[key[0]] - distances[key[1]]);
                const auto   index    = static_cast<uint32_t>(candidate.points.size());
                candidate.points.push_back(input.points[key[0]] + (input.points[key[1]] - input.points[key[0]]) * fraction);
                intersections.emplace(key, index);
                return index;
            };
            for (const auto &face : input.faces) {
                Polygon    clipped;
                const auto append = [&](uint32_t index) {
                    if (clipped.empty() || clipped.back() != index)
                        clipped.push_back(index);
                };
                for (size_t i = 0; i < face.size(); ++i) {
                    const uint32_t a = face[i], b = face[(i + 1) % face.size()];
                    if (distances[a] <= 0.) {
                        append(a);
                        if (distances[a] == 0.)
                            capVertices.insert(a);
                    }
                    if ((distances[a] <= 0.) != (distances[b] <= 0.)) {
                        const auto index = intersection(a, b);
                        append(index);
                        capVertices.insert(index);
                    }
                }
                if (clipped.size() > 1 && clipped.front() == clipped.back())
                    clipped.pop_back();
                if (clipped.size() >= 3 && Length(Area(candidate.points, clipped)) > 1e-14)
                    candidate.faces.push_back(std::move(clipped));
            }
            if (capVertices.size() < 3)
                return false;
            Polygon cap(capVertices.begin(), capVertices.end());
            D3      center{};
            for (const auto vertex : cap)
                center = center + candidate.points[vertex];
            center        = center / static_cast<double>(cap.size());
            const D3 axis = std::abs(plane.normal.x) < .8 ? D3{1., 0., 0.} : D3{0., 1., 0.};
            const D3 u = Unit(Cross(plane.normal, axis)), v = Cross(plane.normal, u);
            std::sort(cap.begin(), cap.end(), [&](uint32_t a, uint32_t b) {
                const D3     pa = candidate.points[a] - center, pb = candidate.points[b] - center;
                const double aa = std::atan2(Dot(pa, v), Dot(pa, u)), ab = std::atan2(Dot(pb, v), Dot(pb, u));
                return aa == ab ? a < b : aa < ab;
            });
            if (Length(Area(candidate.points, cap)) <= 1e-14)
                return false;
            candidate.faces.push_back(std::move(cap));

            std::vector<uint32_t> remap(candidate.points.size(), UINT32_MAX);
            for (const auto &face : candidate.faces)
                for (const auto vertex : face)
                    remap[vertex] = 0;
            Points compact;
            for (size_t i = 0; i < remap.size(); ++i)
                if (remap[i] != UINT32_MAX) {
                    remap[i] = static_cast<uint32_t>(compact.size());
                    compact.push_back(candidate.points[i]);
                }
            if (compact.size() > budget || compact.size() < 4)
                return false;
            for (auto &face : candidate.faces)
                for (auto &vertex : face)
                    vertex = remap[vertex];
            candidate.points = std::move(compact);
            out              = std::move(candidate);
            return true;
        }
        inline bool Triangulate(const Polyhedron &poly, D3 center, std::vector<Tri> &out)
        {
            std::vector<Tri> triangles;
            for (auto face : poly.faces) {
                const D3 normal = Unit(Area(poly.points, face));
                while (face.size() > 3) {
                    const double area     = Dot(Area(poly.points, face), normal);
                    double       best     = 0.;
                    size_t       selected = face.size();
                    for (size_t i = 0; i < face.size(); ++i) {
                        const D3 a = poly.points[face[(i + face.size() - 1) % face.size()]], b = poly.points[face[i]],
                                 c           = poly.points[face[(i + 1) % face.size()]];
                        const double earArea = Dot(Cross(b - a, c - a), normal);
                        if (earArea <= 1e-14 || area - earArea <= 1e-14)
                            continue;
                        const double longest = std::max({Dot(b - a, b - a), Dot(c - a, c - a), Dot(c - b, c - b)});
                        const double quality = earArea / longest;
                        if (quality > best) {
                            best     = quality;
                            selected = i;
                        }
                    }
                    if (selected == face.size())
                        return false;
                    triangles.push_back({face[(selected + face.size() - 1) % face.size()], face[selected], face[(selected + 1) % face.size()]});
                    face.erase(face.begin() + static_cast<std::ptrdiff_t>(selected));
                }
                triangles.push_back({face[0], face[1], face[2]});
            }
            std::map<Edge, std::pair<uint32_t, int>> uses;
            for (const auto &triangle : triangles) {
                if (!(Det(poly.points[triangle[0]] - center, poly.points[triangle[1]] - center, poly.points[triangle[2]] - center) > 1e-14))
                    return false;
                for (size_t i = 0; i < 3; ++i) {
                    const uint32_t a = triangle[i], b = triangle[(i + 1) % 3];
                    auto          &use = uses[EdgeKey(a, b)];
                    ++use.first;
                    use.second += a < b ? 1 : -1;
                }
            }
            for (const auto &[edge, use] : uses) {
                (void)edge;
                if (use.first != 2 || use.second != 0)
                    return false;
            }
            out = std::move(triangles);
            return true;
        }
    } // namespace support_cage

    inline phy::PhysicsStatus
    BuildSupportCage(const Points &source, D3 center, uint32_t shellBudget, const std::atomic_bool *cancel, SupportCage &out)
    {
        using namespace support_cage;
        const auto cancelled = [] { return phy::PhysicsStatus{phy::PhysicsError::InvalidState, "Support cage generation cancelled"}; };
        if (Cancelled(cancel))
            return cancelled();
        if (source.size() < 4 || shellBudget < 8 || !Finite(center))
            return {phy::PhysicsError::InvalidArgument, "Support cage requires a finite center, source and at least eight boundary vertices"};
        D3 low{INFINITY, INFINITY, INFINITY}, high{-INFINITY, -INFINITY, -INFINITY};
        for (const auto point : source) {
            if (!Finite(point))
                return {phy::PhysicsError::InvalidArgument, "Support cage source contains a nonfinite point"};
            low  = Low(low, point);
            high = High(high, point);
        }
        const D3     extent = high - low, origin = (low + high) * .5;
        const double scale = Length(extent), margin = scale * 1e-7;
        if (!std::isfinite(scale) || !(std::min({extent.x, extent.y, extent.z}) > scale * 1e-12))
            return {phy::PhysicsError::Unsupported, "Support cage source bounds are numerically degenerate"};
        const auto normalize = [&](D3 point) {
            const auto delta = point - origin;
            return D3{delta.x / extent.x, delta.y / extent.y, delta.z / extent.z};
        };
        const D3 localCenter = normalize(center), padding{margin / extent.x, margin / extent.y, margin / extent.z};
        Points   normalized;
        normalized.reserve(source.size());
        for (const auto point : source)
            normalized.push_back(normalize(point));
        Polyhedron current;
        for (uint32_t i = 0; i < 8; ++i)
            current.points.push_back(
                {(i & 1 ? 1. : -1.) * (.5 + padding.x), (i & 2 ? 1. : -1.) * (.5 + padding.y), (i & 4 ? 1. : -1.) * (.5 + padding.z)});
        current.faces = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
        if (std::abs(localCenter.x) >= .5 + padding.x || std::abs(localCenter.y) >= .5 + padding.y || std::abs(localCenter.z) >= .5 + padding.z)
            return {phy::PhysicsError::InvalidArgument, "Support cage center is outside the source bounds"};

        std::set<std::array<double, 3>> keys;
        for (const auto direction : Directions()) {
            const D3 worldNormal = Unit({direction.x / extent.x, direction.y / extent.y, direction.z / extent.z});
            keys.insert({std::round(worldNormal.x * 1e9) * 1e-9, std::round(worldNormal.y * 1e9) * 1e-9, std::round(worldNormal.z * 1e9) * 1e-9});
        }
        std::vector<Plane> planes;
        for (const auto &key : keys) {
            if (Cancelled(cancel))
                return cancelled();
            const D3     normal = Unit({key[0], key[1], key[2]});
            const D3     local{normal.x * extent.x, normal.y * extent.y, normal.z * extent.z};
            const double length = Length(local);
            Plane        plane{local / length, -INFINITY};
            for (const auto point : normalized)
                plane.support = std::max(plane.support, Dot(plane.normal, point));
            plane.support += margin / length;
            if (Dot(plane.normal, localCenter) >= plane.support)
                return {phy::PhysicsError::Unsupported, "Support cage center is outside a source supporting plane"};
            planes.push_back(plane);
        }
        std::vector<bool> selected(planes.size(), false);
        double            currentVolume = Volume(current, localCenter);
        for (size_t iteration = 0; iteration < planes.size(); ++iteration) {
            Polyhedron best;
            double     bestVolume = currentVolume;
            size_t     bestIndex  = planes.size();
            for (size_t i = 0; i < planes.size(); ++i) {
                if (Cancelled(cancel))
                    return cancelled();
                if (selected[i])
                    continue;
                Polyhedron candidate;
                if (!Clip(current, planes[i], shellBudget, candidate))
                    continue;
                const double volume = Volume(candidate, localCenter);
                if (std::isfinite(volume) && volume > 0. && volume < bestVolume - 1e-12) {
                    bestVolume = volume;
                    bestIndex  = i;
                    best       = std::move(candidate);
                }
            }
            if (bestIndex == planes.size())
                break;
            current             = std::move(best);
            currentVolume       = bestVolume;
            selected[bestIndex] = true;
        }
        SupportCage candidate;
        if (!Triangulate(current, localCenter, candidate.faces))
            return {phy::PhysicsError::CapacityExceeded, "Support cage has a degenerate or non-manifold boundary"};
        for (const auto point : current.points)
            candidate.nodes.push_back(origin + D3{point.x * extent.x, point.y * extent.y, point.z * extent.z});
        if (Cancelled(cancel))
            return cancelled();
        out = std::move(candidate);
        return {};
    }
} // namespace sky::softbody::detail
