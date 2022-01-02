//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/DeferredRendering.h>
#include <engine/render/RenderView.h>
#include <vulkan/Util.h>

namespace sky {

    static constexpr const char* DirectShadowMap  = "DirectShadowMap";
    static constexpr const char* MainDepthStencil = "MainDepthStencil";
    static constexpr const char* GBufferNormal    = "GBufferNormal";
    static constexpr const char* GBufferAlbedo    = "GBufferAlbedo";


    DeferredRendering::DeferredRendering()
    {
    }

    DeferredRendering::~DeferredRendering()
    {
    }

}