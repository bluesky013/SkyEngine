//
// BuiltinGeometry: assembles math-generated GeometryStreams into a
// RenderGeometry (separate vertex streams POSITION/NORMAL/TANGENT/UV1 +
// index buffer + bounds). The geometry algorithms live in
// core/math/GeometryGenerator.h.
//

#pragma once

#include <aurora/resource/RenderGeometry.h>
#include <core/math/GeometryGenerator.h>
#include <core/shapes/AABB.h>
#include <core/template/ReferenceObject.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace sky::aurora {

    class BuiltinGeometry {
    public:
        static CounterPtr<RenderGeometry> Build(Device *device, const sky::GeometryStreams &streams,
                                                IndexType indexType = IndexType::U32)
        {
            if (device == nullptr || streams.positions.empty()) {
                return nullptr;
            }
            if (indexType == IndexType::U16 && streams.positions.size() >= 65536u) {
                return nullptr;
            }

            auto geo = CounterPtr<RenderGeometry>(new RenderGeometry());
            AddStream<Vector3>(geo, device, streams.positions, VertexSemantic::POSITION);
            AddStream<Vector3>(geo, device, streams.normals, VertexSemantic::NORMAL);
            AddStream<Vector4>(geo, device, streams.tangents, VertexSemantic::TANGENT);
            AddStream<Vector2>(geo, device, streams.uvs, VertexSemantic::UV1);
            AddIndexBuffer(geo, device, streams.indices, indexType);

            geo->SetLocalBounds(ComputeBounds(streams.positions));
            return geo;
        }

    private:
        template <typename T>
        static void AddStream(CounterPtr<RenderGeometry> &geo, Device *device,
                              const std::vector<T> &data, VertexSemantic semantic)
        {
            if (data.empty()) {
                return;
            }
            auto vb = std::make_unique<VertexBuffer>();
            VertexLayout layout;
            layout.stride = sizeof(T);
            layout.semantics.Set(semantic);
            vb->SetLayout(layout);
            if (vb->Init(device, data.size() * sizeof(T))) {
                vb->Upload(data.data(), data.size() * sizeof(T));
                geo->AddVertexStream(std::move(vb));
            }
        }

        static void AddIndexBuffer(CounterPtr<RenderGeometry> &geo, Device *device,
                                   const std::vector<uint32_t> &indices, IndexType indexType)
        {
            if (indices.empty()) {
                return;
            }
            auto ib = std::make_unique<IndexBuffer>();
            ib->SetIndexType(indexType);
            if (indexType == IndexType::U16) {
                std::vector<uint16_t> idx16(indices.size());
                for (size_t i = 0; i < indices.size(); ++i) {
                    idx16[i] = static_cast<uint16_t>(indices[i]);
                }
                if (ib->Init(device, idx16.size() * sizeof(uint16_t))) {
                    ib->Upload(idx16.data(), idx16.size() * sizeof(uint16_t));
                    geo->SetIndexBuffer(std::move(ib));
                }
            } else {
                if (ib->Init(device, indices.size() * sizeof(uint32_t))) {
                    ib->Upload(indices.data(), indices.size() * sizeof(uint32_t));
                    geo->SetIndexBuffer(std::move(ib));
                }
            }
        }

        static AABB ComputeBounds(const std::vector<Vector3> &positions)
        {
            Vector3 lo = positions[0];
            Vector3 hi = positions[0];
            for (size_t i = 1; i < positions.size(); ++i) {
                const Vector3 &p = positions[i];
                lo.x = std::min(lo.x, p.x);
                lo.y = std::min(lo.y, p.y);
                lo.z = std::min(lo.z, p.z);
                hi.x = std::max(hi.x, p.x);
                hi.y = std::max(hi.y, p.y);
                hi.z = std::max(hi.z, p.z);
            }
            return AABB(lo, hi);
        }
    };

} // namespace sky::aurora
