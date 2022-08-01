//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <memory>
#include <vulkan/CommandBuffer.h>
#include <render/RenderPrimtive.h>

namespace sky {

    class FrameGraphEncoder {
    public:
        FrameGraphEncoder() = default;
        virtual ~FrameGraphEncoder() = default;

        virtual void Encode(drv::GraphicsEncoder& encoder) {}
    };

    class FrameGraphRasterEncoder : public FrameGraphEncoder {
    public:
        FrameGraphRasterEncoder() = default;
        ~FrameGraphRasterEncoder() = default;

        void Encode(drv::GraphicsEncoder& encoder) override;

        void Emplace(const drv::DrawItem& item);

        void SetDrawTag(uint32_t tag);

        uint32_t GetDrawTag() const;

    private:
        uint32_t drawTag {0};
        std::vector<drv::DrawItem> drawItems;
    };

}
