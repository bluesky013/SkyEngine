#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

namespace sky::softbody::detail {
    struct D3 {
        double x = 0., y = 0., z = 0.;
        double operator[](size_t i) const
        {
            return i == 0 ? x : i == 1 ? y : z;
        }
        D3 operator+(D3 b) const
        {
            return {x + b.x, y + b.y, z + b.z};
        }
        D3 operator-(D3 b) const
        {
            return {x - b.x, y - b.y, z - b.z};
        }
        D3 operator*(double s) const
        {
            return {x * s, y * s, z * s};
        }
        D3 operator/(double s) const
        {
            return *this * (1. / s);
        }
    };
    inline double Dot(D3 a, D3 b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    inline D3 Cross(D3 a, D3 b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    inline double Length(D3 a)
    {
        return std::sqrt(Dot(a, a));
    }
    inline D3 Unit(D3 a)
    {
        return a / Length(a);
    }
    inline double Det(D3 a, D3 b, D3 c)
    {
        return Dot(a, Cross(b, c));
    }
    inline bool Finite(D3 p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }
    using Tri    = std::array<uint32_t, 3>;
    using Tet    = std::array<uint32_t, 4>;
    using Points = std::vector<D3>;
    inline D3 Low(D3 a, D3 b)
    {
        return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    }
    inline D3 High(D3 a, D3 b)
    {
        return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
    }
    inline D3 SolveColumns(D3 a, D3 b, D3 c, D3 p)
    {
        const double determinant = Det(a, b, c);
        return {Det(p, b, c) / determinant, Det(a, p, c) / determinant, Det(a, b, p) / determinant};
    }
    inline std::array<double, 4> Barycentric(const Points &points, Tet t, D3 p)
    {
        const D3 a = points[t[0]];
        const D3 q = SolveColumns(points[t[1]] - a, points[t[2]] - a, points[t[3]] - a, p - a);
        return {1. - q.x - q.y - q.z, q.x, q.y, q.z};
    }
    inline double Volume6(const Points &points, Tet t)
    {
        return Det(points[t[1]] - points[t[0]], points[t[2]] - points[t[0]], points[t[3]] - points[t[0]]);
    }
    inline double PointTriangleDistance2(D3 p, D3 a, D3 b, D3 c)
    {
        const D3     ab = b - a, ac = c - a, ap = p - a;
        const double d1 = Dot(ab, ap), d2 = Dot(ac, ap);
        if (d1 <= 0. && d2 <= 0.)
            return Dot(ap, ap);
        const D3     bp = p - b;
        const double d3 = Dot(ab, bp), d4 = Dot(ac, bp);
        if (d3 >= 0. && d4 <= d3)
            return Dot(bp, bp);
        const double vc = d1 * d4 - d3 * d2;
        if (vc <= 0. && d1 >= 0. && d3 <= 0.) {
            const D3 q = p - (a + ab * (d1 / (d1 - d3)));
            return Dot(q, q);
        }
        const D3     cp = p - c;
        const double d5 = Dot(ab, cp), d6 = Dot(ac, cp);
        if (d6 >= 0. && d5 <= d6)
            return Dot(cp, cp);
        const double vb = d5 * d2 - d1 * d6;
        if (vb <= 0. && d2 >= 0. && d6 <= 0.) {
            const D3 q = p - (a + ac * (d2 / (d2 - d6)));
            return Dot(q, q);
        }
        const double va = d3 * d6 - d5 * d4;
        if (va <= 0. && d4 >= d3 && d5 >= d6) {
            const D3 q = p - (b + (c - b) * ((d4 - d3) / (d4 - d3 + d5 - d6)));
            return Dot(q, q);
        }
        const double reciprocal = 1. / (va + vb + vc);
        const D3     q          = p - (a + ab * (vb * reciprocal) + ac * (vc * reciprocal));
        return Dot(q, q);
    }

    class TriangleBvh {
    public:
        TriangleBvh(const Points &vertices, const std::vector<Tri> &triangles) : points(vertices), faces(triangles)
        {
            order.resize(faces.size());
            std::iota(order.begin(), order.end(), 0u);
            Build(0, static_cast<uint32_t>(order.size()));
        }
        double Distance(D3 point) const
        {
            double                best = std::numeric_limits<double>::infinity();
            std::vector<uint32_t> pending{0};
            while (!pending.empty()) {
                const uint32_t index = pending.back();
                pending.pop_back();
                const auto &node = nodes[index];
                if (BoxDistance(point, node) >= best)
                    continue;
                if (node.count) {
                    for (uint32_t i = node.first; i < node.first + node.count; ++i) {
                        const auto &f = faces[order[i]];
                        best          = std::min(best, PointTriangleDistance2(point, points[f[0]], points[f[1]], points[f[2]]));
                    }
                } else {
                    uint32_t a = node.left, b = node.right;
                    if (BoxDistance(point, nodes[a]) < BoxDistance(point, nodes[b]))
                        std::swap(a, b);
                    pending.push_back(a);
                    pending.push_back(b);
                }
            }
            return std::sqrt(best);
        }

    private:
        struct Node {
            D3       low, high;
            uint32_t first = 0, count = 0, left = 0, right = 0;
        };
        static double BoxDistance(D3 p, const Node &node)
        {
            const D3 d{std::max({node.low.x - p.x, p.x - node.high.x, 0.}), std::max({node.low.y - p.y, p.y - node.high.y, 0.}),
                       std::max({node.low.z - p.z, p.z - node.high.z, 0.})};
            return Dot(d, d);
        }
        uint32_t Build(uint32_t first, uint32_t count)
        {
            Node node;
            node.first = first;
            node.count = count;
            node.low   = {INFINITY, INFINITY, INFINITY};
            node.high  = {-INFINITY, -INFINITY, -INFINITY};
            for (uint32_t i = first; i < first + count; ++i)
                for (const uint32_t p : faces[order[i]]) {
                    node.low  = Low(node.low, points[p]);
                    node.high = High(node.high, points[p]);
                }
            const uint32_t index = static_cast<uint32_t>(nodes.size());
            nodes.push_back(node);
            if (count > 8) {
                const D3       extent = node.high - node.low;
                const size_t   axis   = extent.x >= extent.y && extent.x >= extent.z ? 0 : extent.y >= extent.z ? 1 : 2;
                const uint32_t middle = first + count / 2;
                std::nth_element(order.begin() + first, order.begin() + middle, order.begin() + first + count, [&](uint32_t a, uint32_t b) {
                    double ca = 0., cb = 0.;
                    for (size_t k = 0; k < 3; ++k) {
                        ca += points[faces[a][k]][axis];
                        cb += points[faces[b][k]][axis];
                    }
                    return ca == cb ? a < b : ca < cb;
                });
                const uint32_t left = Build(first, count / 2), right = Build(middle, count - count / 2);
                nodes[index].left  = left;
                nodes[index].right = right;
                nodes[index].count = 0;
            }
            return index;
        }
        const Points           &points;
        const std::vector<Tri> &faces;
        std::vector<uint32_t>   order;
        std::vector<Node>       nodes;
    };
} // namespace sky::softbody::detail
