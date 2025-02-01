//
// Created by Zach Lee on 2023/6/9.
//

#include <mtl/VertexAssembly.h>
#include <mtl/Buffer.h>

namespace sky::mtl {

    bool VertexAssembly::Init(const Descriptor &desc)
    {
        vertexBuffers.reserve(desc.vertexBuffers.size());
        mtlVertexBuffers.reserve(desc.vertexBuffers.size());
        vertexOffsets.reserve(desc.vertexBuffers.size());
        for (auto &vb : desc.vertexBuffers) {
            vertexBuffers.emplace_back(vb);
            auto &mtlVb = vertexBuffers.back();

            auto *buffer = static_cast<Buffer*>(mtlVb.buffer.get());
            mtlVertexBuffers.emplace_back(buffer->GetNativeHandle());
            vertexOffsets.emplace_back(mtlVb.offset);
        }
        range = NSMakeRange(0, vertexBuffers.size());
        return true;
    }

    void VertexAssembly::OnBind(id<MTLRenderCommandEncoder> encoder)
    {
        [encoder setVertexBuffers: mtlVertexBuffers.data()
                          offsets: vertexOffsets.data()
                        withRange: range];
    }

} // namespace sky::mtl
