//
// Created by Zach Lee on 2022/8/1.
//

#include <render/RenderEncoder.h>

namespace sky {

    void RenderRasterEncoder::Encode(drv::GraphicsEncoder& encoder)
    {
        for (auto& producer : producers) {
            producer->Process(encoder);
        }
        producers.clear();
    }

    void RenderRasterEncoder::Emplace(const drv::DrawItem& item)
    {
        producers.emplace_back(new ItemDrawCallProducer(item)); // TODO : Pool
    }

    void RenderRasterEncoder::SetDrawTag(uint32_t tag)
    {
        drawTag = tag;
    }

    uint32_t RenderRasterEncoder::GetDrawTag() const
    {
        return drawTag;
    }

    void ItemDrawCallProducer::Process(drv::GraphicsEncoder& encoder)
    {
        encoder.Encode(item);
    }
}