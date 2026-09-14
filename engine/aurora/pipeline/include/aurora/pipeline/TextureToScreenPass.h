//
// Texture to screen pass: fullscreen triangle sampling an input HDR texture
// into a backbuffer color attachment.
//

#pragma once

#include <aurora/pipeline/FullScreenPass.h>

namespace sky::aurora {

    class TextureToScreenPass : public FullScreenPass {
    public:
        TextureToScreenPass();
        ~TextureToScreenPass() override = default;

        void OnSetup(Device *device) override;
        void BuildRDG(RenderGraph &graph) override;
        void OnSceneChanged() override;

    protected:
        const std::vector<RgBlockDesc> &GetPassBlocks() const override;
    };

} // namespace sky::aurora
