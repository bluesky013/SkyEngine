//
// Texture to screen pass: fullscreen triangle sampling an input HDR texture
// into a backbuffer color attachment.
//

#include <aurora/pipeline/TextureToScreenPass.h>

namespace sky::aurora {

    namespace {
        std::vector<RgBlockDesc> MakeTextureToScreenBlocks()
        {
            // pass tier (set 1): the input texture SRV descriptor is written
            // per-frame once the transient backing image is resolved
            RgBlockDesc block{};
            block.set       = 1;
            block.binding   = 0;
            block.blockName = Name("TextureToScreenParams");
            block.kind      = RgBlockKind::CBUFFER;
            block.fields    = {
                {RgFieldType::FLOAT4, Name("Misc")}, // placeholder params
            };
            return {block};
        }
    } // namespace

    TextureToScreenPass::TextureToScreenPass() : FullScreenPass(Name("TextureToScreenPass"))
    {
    }

    const std::vector<RgBlockDesc> &TextureToScreenPass::GetPassBlocks() const
    {
        static const std::vector<RgBlockDesc> blocks = MakeTextureToScreenBlocks();
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
