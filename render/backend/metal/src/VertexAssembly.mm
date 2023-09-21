//
// Created by Zach Lee on 2023/6/9.
//

#include <mtl/VertexAssembly.h>

namespace sky::mtl {

    bool VertexAssembly::Init(const Descriptor &desc)
    {
        vertexBuffers.reserve(desc.vertexBuffers.size());
        mtlVertexBuffers.reserve(desc.vertexBuffers.size());
        vertexOffsets.reserve(desc.vertexBuffers.size());
        for (auto &vb : desc.vertexBuffers) {
            vertexBuffers.emplace_back(std::static_pointer_cast<BufferView>(vb));
            auto &mtlVb = vertexBuffers.back();

            mtlVertexBuffers.emplace_back(mtlVb->GetNativeHandle());
            vertexOffsets.emplace_back(mtlVb->GetViewDesc().offset);
        }
        range = NSMakeRange(0, vertexBuffers.size());

        if (desc.indexBuffer) {
            indexBuffer = std::static_pointer_cast<BufferView>(desc.indexBuffer);
            mtlIndexBuffer = indexBuffer->GetNativeHandle();
            indexType = desc.indexType == rhi::IndexType::U16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
        }

        return true;
    }

    void VertexAssembly::OnBind(id<MTLRenderCommandEncoder> encoder)
    {
        [encoder setVertexBuffers: mtlVertexBuffers.data()
                          offsets: vertexOffsets.data()
                        withRange: range];
    }

} // namespace sky::mtl
