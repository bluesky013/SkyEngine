//
// Opaque pass: color + depth attachments, single "opaque" queue (front-to-back).
//

#pragma once

#include <aurora/pipeline/SceneRasterPassTemplate.h>
#include <aurora/rdg/RDGTypes.h>

namespace sky::aurora {

    class OpaquePass : public SceneRasterPassTemplate {
    public:
        OpaquePass();
        ~OpaquePass() override = default;

        void OnSetup(Device *device) override;
        void BuildRDG(RenderGraph &graph) override;
        void OnSceneChanged() override;

        void SetExtent(uint32_t width, uint32_t height)
        {
            mWidth  = width;
            mHeight = height;
        }

        RDGTextureHandle GetColorHandle() const { return mColor; }
        RDGTextureHandle GetDepthHandle() const { return mDepth; }

    private:
        uint32_t mWidth  = 0;
        uint32_t mHeight = 0;

        RDGTextureHandle mColor;
        RDGTextureHandle mDepth;
    };

} // namespace sky::aurora
