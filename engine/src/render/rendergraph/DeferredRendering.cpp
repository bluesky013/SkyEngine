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
        : viewTags{"MainCamera, DirectShadow"}
        , imageDes{
            { DirectShadowMap, drv::MakeImage2D(VK_FORMAT_D32_SFLOAT, {1024, 1024}) },
            { MainDepthStencil, drv::MakeImage2D(VK_FORMAT_D24_UNORM_S8_UINT, { 1, 1}) },
            { GBufferNormal, drv::MakeImage2D(VK_FORMAT_R8G8B8_UNORM, {1, 1}) },
            { GBufferAlbedo, drv::MakeImage2D(VK_FORMAT_R8G8B8_UNORM, {1, 1}) }
          }
        , resizable {
            MainDepthStencil, GBufferNormal, GBufferAlbedo
          }
    {
    }

    bool DeferredRendering::HasViewTags(const std::string& tag) const
    {
        return viewTags.count(tag);
    }

    void DeferredRendering::BuildResources()
    {
        RegisterImage("DirectShadow", "DirectShadowMap");
        RegisterImage("MainCamera", "MainDepthStencil");
        RegisterImage("MainCamera", "GBufferNormal");
        RegisterImage("MainCamera", "GBufferAlbedo");
    }

    void DeferredRendering::SetOutputConfig(const GraphOutput& config)
    {
        for (auto& cfg : resizable) {
            auto& des = imageDes[cfg];
            des.format = config.format;
            des.extent.width = config.width;
            des.extent.height = config.height;
        }
    }

    void DeferredRendering::PreparePipeline(RenderGraphBuilder& builder, std::list<RenderView*>& views)
    {
        for (auto view : views) {
            auto& des = view->GetDescriptor();
            if (des.viewTag == "DirectShadow") {

            }
        }
    }

}