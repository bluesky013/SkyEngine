//
// Created by Zach Lee on 2022/8/1.
//

#include <render/framegraph/FrameGraphEncoder.h>

namespace sky {

    void FrameGraphRasterEncoder::Encode(drv::GraphicsEncoder& encoder)
    {
        for (auto& item : drawItems) {
            encoder.Encode(item);
        }
        drawItems.clear();
    }

    void FrameGraphRasterEncoder::Emplace(const drv::DrawItem& item)
    {
        drawItems.emplace_back(item);
    }

}