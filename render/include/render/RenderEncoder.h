//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <memory>
#include <vector>
#include <vulkan/CommandBuffer.h>

namespace sky {

    class RenderEncoder {
    public:
        RenderEncoder() = default;
        virtual ~RenderEncoder() = default;

        virtual void Encode(drv::GraphicsEncoder& encoder) {}
    };

    class RenderRasterEncoder : public RenderEncoder {
    public:
        RenderRasterEncoder() = default;
        ~RenderRasterEncoder() = default;

        void Encode(drv::GraphicsEncoder& encoder) override;

        void Emplace(const drv::DrawItem& item);

        void SetDrawTag(uint32_t tag);

        uint32_t GetDrawTag() const;

    private:
        uint32_t drawTag {0};
        std::vector<drv::DrawItem> drawItems;
    };

}
