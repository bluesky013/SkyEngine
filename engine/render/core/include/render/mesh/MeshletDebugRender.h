//
// Created by blues on 2025/2/7.
//

#pragma once

#include <render/RenderPrimitive.h>

namespace sky {

    struct MeshletConePrimitive : public RenderPrimitive {
        static constexpr uint32_t CONE_SEGMENT = 32;
    };

    class MeshletDebugRender {
    public:
        MeshletDebugRender() = default;
        ~MeshletDebugRender() = default;

        void Setup(const RDBufferPtr &meshlet);
        MeshletConePrimitive* GetPrimitive() const { return conePrimitive.get(); }
    private:
        void BuildGeometry(const RDBufferPtr &meshlet);
        void BuildBatch();

        std::unique_ptr<MeshletConePrimitive> conePrimitive;
    };

} // namespace sky