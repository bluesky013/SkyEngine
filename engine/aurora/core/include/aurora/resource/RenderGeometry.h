//
// RenderGeometry: composite render resource bundling vertex streams, an
// optional index buffer, and local bounds. Pure wrapper — the caller creates
// and uploads the VertexBuffer/IndexBuffer resources, then hands them over;
// the geometry owns them and exposes accessors for later DrawItem binding.
//

#pragma once

#include <aurora/resource/Buffer.h>
#include <core/name/Name.h>
#include <core/shapes/AABB.h>
#include <core/template/ReferenceObject.h>

#include <memory>
#include <vector>

namespace sky::aurora {

    class RenderGeometry : public RefObject {
    public:
        RenderGeometry() = default;
        explicit RenderGeometry(const Name &inName) : name(inName) {}
        ~RenderGeometry() override = default;

        RenderGeometry(const RenderGeometry &) = delete;
        RenderGeometry &operator=(const RenderGeometry &) = delete;

        void SetLocalBounds(const AABB &bounds)
        {
            localBounds = bounds;
        }

        void AddVertexStream(std::unique_ptr<VertexBuffer<>> vb)
        {
            if (vb != nullptr) {
                vertexStreams.push_back(std::move(vb));
            }
        }

        void SetIndexBuffer(std::unique_ptr<IndexBuffer<>> ib)
        {
            indexBuffer = std::move(ib);
        }

        const std::vector<std::unique_ptr<VertexBuffer<>>> &GetVertexStreams() const
        {
            return vertexStreams;
        }

        IndexBuffer<> *GetIndexBuffer() const
        {
            return indexBuffer.get();
        }

        const AABB &GetLocalBounds() const
        {
            return localBounds;
        }

        const Name &GetName() const
        {
            return name;
        }

    private:
        Name                                      name;
        std::vector<std::unique_ptr<VertexBuffer<>>> vertexStreams;
        std::unique_ptr<IndexBuffer<>>             indexBuffer;
        AABB                                       localBounds{};
    };

} // namespace sky::aurora
