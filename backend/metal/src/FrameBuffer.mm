//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/FrameBuffer.h>
#include <mtl/Device.h>

namespace sky::mtl {

    bool FrameBuffer::Init(const Descriptor &desc)
    {
        renderPass = std::static_pointer_cast<RenderPass>(desc.pass);
        attachments.reserve(desc.views.size());
        for (auto &view : desc.views) {
            attachments.emplace_back(std::static_pointer_cast<ImageView>(view));
        }

        return true;
    }

} // namespace sky::mtl
