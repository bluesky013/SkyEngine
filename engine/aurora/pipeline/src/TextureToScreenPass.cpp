//
// Texture to screen pass: fullscreen triangle sampling an input HDR texture
// into a backbuffer color attachment.
//

#include "TextureToScreenPass.h"
#include <aurora/pipeline/TextureToScreenPass.h>

namespace sky::aurora {

    TextureToScreenPass::TextureToScreenPass() : FullScreenPass(Name("TextureToScreenPass"))
    {
    }

    const std::vector<RgBlockDesc> &TextureToScreenPass::GetPassBlocks() const
    {
        static const std::vector<RgBlockDesc> blocks{shared::GetTextureToScreenParamsBlockDesc()};
        return blocks;
    }

    void TextureToScreenPass::OnSetup(Device *device)
    {
        FullScreenPass::OnSetup(device);
        // TODO: create persistent fullscreen PSO + pass RG (set 1) sampling the
        // input texture, once shader pipeline is wired in
    }

    void TextureToScreenPass::OnSceneChanged()
    {
        FullScreenPass::OnSceneChanged();
    }

    void TextureToScreenPass::BuildRDG(RenderGraph &graph)
    {
        BuildFullScreenPass(graph, LoadOp::DONT_CARE, StoreOp::STORE);
    }

} // namespace sky::aurora
