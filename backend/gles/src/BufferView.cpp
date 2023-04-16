//
// Created by Zach on 2023/1/31.
//

#include <gles/BufferView.h>
#include <gles/Device.h>

namespace sky::gles {

    bool BufferView::Init(const rhi::BufferViewDesc &desc)
    {
        viewDesc = desc;
        return true;
    }

    uint8_t *BufferView::Map()
    {
        CHECK(glBindBuffer(source->GetGLTarget(), source->GetNativeHandle()));
        return static_cast<uint8_t *>(glMapBufferRange(source->GetGLTarget(), viewDesc.offset, viewDesc.range, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
    }

    void BufferView::UnMap()
    {
        CHECK(glUnmapBuffer(source->GetGLTarget()));
        CHECK(glBindBuffer(source->GetGLTarget(), 0));
    }

    std::shared_ptr<rhi::BufferView> BufferView::CreateView(const rhi::BufferViewDesc &desc) const
    {
        return source->CreateView(desc);
    }

}
