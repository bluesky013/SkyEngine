//
// Created by blues on 2024/12/8.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    /**
     * Screen-space decal pass. Decoupled from forward/deferred pipelines:
     * it only requires a depth buffer resource name and a color render target
     * resource name which can be provided by any opaque pass.
     *
     * Algorithm:
     *  1. Full-screen triangle reconstructs world position from depth using InvViewProj.
     *  2. For each active decal the world position is transformed into the decal's
     *     OBB local space (WorldToDecal matrix).  If the local position lies inside
     *     the unit cube [-0.5, 0.5]³ the decal colour is alpha-blended onto the output.
     */
    class DecalPass : public FullScreenPass {
    public:
        /**
         * @param tech       Pre-loaded decal technique (decal.tech).
         * @param colorName  Name of the colour render target to write into (LoadOp::LOAD).
         * @param depthName  Name of the scene depth texture to read from (SRV).
         */
        DecalPass(const RDGfxTechPtr &tech, const Name &colorName, const Name &depthName);
        ~DecalPass() override = default;

        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;

    private:
        RDResourceLayoutPtr decalLayout;
    };

} // namespace sky
