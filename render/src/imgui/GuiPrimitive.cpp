//
// Created by Zach Lee on 2022/8/20.
//

#include <render/imgui/GuiPrimitive.h>
#include <render/RenderEncoder.h>

namespace sky {

    void GuiPrimitive::SetDrawTag(uint32_t tag)
    {
        drawTag = tag;
    }

    void GuiPrimitive::Encode(RenderRasterEncoder* encoder)
    {
        if ((encoder->GetDrawTag() & drawTag) == 0) {
            return;
        }

        for (auto& arg : args) {
            drv::DrawItem item;
            item.pso = pso;
            item.vertexAssembly = assembly;
            item.drawArgs = arg;
            item.shaderResources = setBinder;
            item.pushConstants = constants;
            encoder->Emplace(item);
        }
    }

}