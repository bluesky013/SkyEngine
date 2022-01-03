//
// Created by Zach Lee on 2021/12/22.
//

#pragma once

#include <vulkan/Image.h>
#include <vulkan/RenderPass.h>
#include <vulkan/FrameBuffer.h>
#include <engine/render/rendergraph/RenderGraphNode.h>

namespace sky {

    class RenderGraphResource : public RenderGraphNode {
    public:
        RenderGraphResource(std::string&& str) : RenderGraphNode(std::forward<std::string>(str)) {}
        ~RenderGraphResource() = default;
    };

    class GraphImage : public RenderGraphResource {
    public:
        GraphImage(std::string str)
            : RenderGraphResource(std::move(str))
        {
        }

        ~GraphImage()
        {
        }

        bool Init(const drv::Image::Descriptor& des);

        bool operator==(const drv::Image::Descriptor& des);

        drv::ImagePtr GetImage() const;
    private:
        drv::Image::Descriptor descriptor;
        drv::ImagePtr image;
    };

    class GraphAttachment : public RenderGraphResource {
    public:
        GraphAttachment(std::string str, drv::ImagePtr img)
            : RenderGraphResource(std::move(str))
            , image(img)
        {
        }

        ~GraphAttachment()
        {
        }

        bool Init(const drv::ImageView::Descriptor& des);

        bool Compile();

        bool operator==(const drv::ImageView::Descriptor& des);

        const drv::ImageViewPtr& GetImageView() const;

    private:
        drv::ImageView::Descriptor descriptor;
        drv::ImagePtr image;
        drv::ImageViewPtr view;
    };

    using RGResourcePtr = std::shared_ptr<RenderGraphResource>;
    using RGImagePtr = std::shared_ptr<GraphImage>;
    using RGAttachmentPtr = std::shared_ptr<GraphAttachment>;
}