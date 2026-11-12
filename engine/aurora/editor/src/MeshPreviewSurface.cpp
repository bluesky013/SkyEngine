#include "MeshPreviewSurface.h"

#include <editor/core/viewport/EditorCamera.h>
#include <editor/ui/UiTheme.h>
#include <physics/softbody/IMeshAuthoringCompiler.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

namespace sky::aurora::editor {
    namespace {
        struct Vertex {
            Vector3 view;
            float   value;
        };

        bool Finite(const Vector3 &p)
        {
            return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
        }

        using Polygon = std::array<Vertex, 6>;

        size_t ClipDepth(std::span<const Vertex> input, Polygon &output, float plane, bool keepAbove)
        {
            if (input.empty())
                return 0;
            size_t count    = 0;
            Vertex previous = input.back();
            for (const auto &v : input) {
                const bool inside = keepAbove ? v.view.z >= plane : v.view.z <= plane;
                // Strict crossing avoids adding a vertex on the plane twice.
                if ((previous.view.z < plane && v.view.z > plane) || (previous.view.z > plane && v.view.z < plane)) {
                    const double t = (static_cast<double>(plane) - previous.view.z) / (static_cast<double>(v.view.z) - previous.view.z);
                    Vertex       intersection;
                    intersection.view  = {static_cast<float>(std::lerp(static_cast<double>(previous.view.x), static_cast<double>(v.view.x), t)),
                                          static_cast<float>(std::lerp(static_cast<double>(previous.view.y), static_cast<double>(v.view.y), t)),
                                          plane};
                    intersection.value = std::lerp(previous.value, v.value, static_cast<float>(t));
                    output[count++]    = intersection;
                }
                if (inside)
                    output[count++] = v;
                previous = v;
            }
            return count;
        }

        struct Projected {
            double x, y, inverseDepth, valueOverDepth;
        };

        struct Edge {
            explicit Edge(const Projected &a, const Projected &b) : dx(a.y - b.y), dy(b.x - a.x), offset(a.x * b.y - b.x * a.y)
            {
            }
            double At(double x, double y) const
            {
                return dx * x + dy * y + offset;
            }
            double dx, dy, offset;
        };

        Vector3 ColorChannels(uint32_t rgba)
        {
            return {static_cast<float>(rgba & 255), static_cast<float>((rgba >> 8) & 255), static_cast<float>((rgba >> 16) & 255)};
        }

        struct Surface {
            ui::UIImageData    image;
            std::vector<float> depth;
            Vector3            low, high;
            double             focalLength;

            Projected Project(const Vertex &v) const
            {
                const double inverseDepth = 1.0 / v.view.z;
                return {image.width * 0.5 + v.view.x * focalLength * inverseDepth, image.height * 0.5 - v.view.y * focalLength * inverseDepth,
                        inverseDepth, v.value * inverseDepth};
            }

            void Triangle(const Vertex &a, const Vertex &b, const Vertex &c)
            {
                const auto   p = Project(a), q = Project(b), r = Project(c);
                const Edge   edges[] = {Edge(q, r), Edge(r, p), Edge(p, q)};
                const double area    = edges[0].At(p.x, p.y);
                if (!std::isfinite(area) || std::abs(area) < 1.e-10)
                    return;
                const double left   = std::max(0., std::ceil(std::min({p.x, q.x, r.x}) - 0.5));
                const double top    = std::max(0., std::ceil(std::min({p.y, q.y, r.y}) - 0.5));
                const double right  = std::min(static_cast<double>(image.width - 1), std::floor(std::max({p.x, q.x, r.x}) - 0.5));
                const double bottom = std::min(static_cast<double>(image.height - 1), std::floor(std::max({p.y, q.y, r.y}) - 0.5));
                if (left > right || top > bottom)
                    return;
                const double sign = std::copysign(1., area), inverseArea = 1. / area;
                for (uint32_t y = static_cast<uint32_t>(top); y <= static_cast<uint32_t>(bottom); ++y) {
                    double u = edges[0].At(left + 0.5, y + 0.5);
                    double v = edges[1].At(left + 0.5, y + 0.5);
                    double w = edges[2].At(left + 0.5, y + 0.5);
                    for (uint32_t x = static_cast<uint32_t>(left); x <= static_cast<uint32_t>(right);
                         ++x, u += edges[0].dx, v += edges[1].dx, w += edges[2].dx) {
                        if (sign * u < 0. || sign * v < 0. || sign * w < 0.)
                            continue;
                        const auto   offset       = static_cast<size_t>(y) * image.width + x;
                        const double depthSum     = u * p.inverseDepth + v * q.inverseDepth + w * r.inverseDepth;
                        const float  inverseDepth = static_cast<float>(depthSum * inverseArea);
                        if (inverseDepth <= depth[offset])
                            continue;
                        depth[offset]       = inverseDepth;
                        const float   value = static_cast<float>((u * p.valueOverDepth + v * q.valueOverDepth + w * r.valueOverDepth) / depthSum);
                        const Vector3 color = low + (high - low) * std::clamp(value, 0.f, 1.f);
                        auto         *pixel = image.pixels.data() + offset * 4;
                        pixel[0]            = static_cast<uint8_t>(color.x);
                        pixel[1]            = static_cast<uint8_t>(color.y);
                        pixel[2]            = static_cast<uint8_t>(color.z);
                        pixel[3]            = 255;
                    }
                }
            }
        };

    } // namespace

    ui::UIImageData RasterizeMeshWeights(const phy::AuthoringMeshSource  &mesh,
                                         const phy::AuthoringMeshBinding &binding,
                                         const sky::editor::EditorCamera &camera,
                                         uint32_t                         width,
                                         uint32_t                         height,
                                         uint32_t                         bone)
    {
        const uint64_t pixels = static_cast<uint64_t>(width) * height;
        if (!pixels || pixels > 4u * 1024 * 1024 || !(camera.nearPlane > 0 && camera.farPlane > camera.nearPlane))
            return {};
        const double focalLength = 0.5 * height / std::tan(0.5 * camera.verticalFov);
        if (!std::isfinite(focalLength) || focalLength <= 0.)
            return {};
        const Vector3 right = camera.Right(), up = camera.Up(), forward = camera.Forward();
        const auto   &colors = sky::editor::GetDefaultUiTheme().colors;
        Surface       surface;
        surface.image.width  = width;
        surface.image.height = height;
        surface.image.pixels.resize(static_cast<size_t>(pixels) * 4);
        surface.depth.resize(static_cast<size_t>(pixels));
        surface.focalLength = focalLength;
        surface.low         = ColorChannels(colors.gizmoZ);
        surface.high        = ColorChannels(colors.gizmoX);
        std::vector<Vertex> vertices;
        for (const auto &primitive : mesh.primitives) {
            const auto skin =
                std::find_if(binding.primitives.begin(), binding.primitives.end(), [&](const auto &p) { return p.key == primitive.key; });
            if (skin == binding.primitives.end() || skin->vertices.size() != primitive.positions.size())
                return {};
            vertices.resize(primitive.positions.size());
            for (size_t i = 0; i < vertices.size(); ++i) {
                const auto   &p     = primitive.positions[i];
                const auto    model = primitive.transform * Vector4(p.x, p.y, p.z, 1.f);
                const Vector3 delta = Vector3(model.x, model.y, model.z) - camera.position;
                auto         &v     = vertices[i];
                v.view              = {delta.Dot(right), delta.Dot(up), delta.Dot(forward)};
                const auto &weights = skin->vertices[i];
                v.value             = 0.f;
                for (size_t j = 0; j < weights.bones.size(); ++j)
                    if (weights.bones[j] == bone && std::isfinite(weights.weights[j]))
                        v.value += weights.weights[j];
                v.value = std::clamp(v.value, 0.f, 1.f);
            }
            for (size_t i = 0; i + 2 < primitive.indices.size(); i += 3) {
                const auto *ids = primitive.indices.data() + i;
                if (ids[0] >= vertices.size() || ids[1] >= vertices.size() || ids[2] >= vertices.size())
                    continue;
                Polygon       polygon{vertices[ids[0]], vertices[ids[1]], vertices[ids[2]]};
                const Vertex &a = polygon[0], &b = polygon[1], &c = polygon[2];
                if (!Finite(a.view) || !Finite(b.view) || !Finite(c.view))
                    continue;
                const float nearest = std::min({a.view.z, b.view.z, c.view.z}), farthest = std::max({a.view.z, b.view.z, c.view.z});
                if (farthest < camera.nearPlane || nearest > camera.farPlane)
                    continue;
                size_t  count = 3;
                Polygon clipped{};
                if (nearest < camera.nearPlane) {
                    count   = ClipDepth(std::span(polygon).first(count), clipped, camera.nearPlane, true);
                    polygon = clipped;
                }
                if (farthest > camera.farPlane) {
                    count   = ClipDepth(std::span(polygon).first(count), clipped, camera.farPlane, false);
                    polygon = clipped;
                }
                for (size_t j = 2; j < count; ++j)
                    surface.Triangle(polygon[0], polygon[j - 1], polygon[j]);
            }
        }
        return std::move(surface.image);
    }
} // namespace sky::aurora::editor
