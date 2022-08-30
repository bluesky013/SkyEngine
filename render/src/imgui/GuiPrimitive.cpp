//
// Created by Zach Lee on 2022/8/20.
//

#include <render/RenderEncoder.h>
#include <render/imgui/GuiPrimitive.h>

namespace sky {

    void GuiPrimitive::SetDrawTag(uint32_t tag)
    {
        drawTag = tag;
    }

    void GuiPrimitive::Encode(RenderRasterEncoder *encoder)
    {
        if ((encoder->GetDrawTag() & drawTag) == 0) {
            return;
        }

        encoder->EmplaceLambda([this](drv::GraphicsEncoder &gfx) {
            gfx.BindPipeline(pso);
            gfx.BindAssembly(assembly);
            gfx.BindShaderResource(setBinder);
            gfx.PushConstant(constants);
            for (auto &drawCall : dc) {
                gfx.SetScissor(1, &drawCall.scissor);
                gfx.DrawIndexed(drawCall.indexed);
            }
        });
    }
} // namespace sky