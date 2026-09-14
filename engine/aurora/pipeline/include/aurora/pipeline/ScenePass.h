//
// Scene pass: HDR color + depth attachments, single "opaque" queue (front-to-back).
//

#pragma once

#include <aurora/pipeline/SceneRasterPassTemplate.h>
#include <aurora/rdg/RDGTypes.h>

namespace sky::aurora {

    class ScenePass : public SceneRasterPassTemplate {
    public:
        ScenePass();
        ~ScenePass() override = default;

        void OnSetup(Device *device) override;
        void BuildRDG(RenderGraph &graph) override;
        void OnSceneChanged() override;

        void SetExtent(uint32_t width, uint32_t height)
        {
            mWidth  = width;
            mHeight = height;
        }

        RDGTextureHandle GetHDRColorHandle() const { return mHdrColor; }
        RDGTextureHandle GetDepthHandle() const { return mDepth; }

    protected:
        const std::vector<RgBlockDesc> &GetPassBlocks() const override;

    private:
        uint32_t mWidth  = 0;
        uint32_t mHeight = 0;

        RDGTextureHandle mHdrColor;
        RDGTextureHandle mDepth;
    };

} // namespace sky::aurora
