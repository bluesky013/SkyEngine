//
// Created by Zach Lee on 2021/12/22.
//

#pragma once

#include <vulkan/Image.h>
#include <engine/render/rendergraph/RenderGraphNode.h>

namespace sky {

    enum class SizePolicy : uint8_t {
        CUSTOM,
        INPUT
    };

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

        ~GraphImage() = default;

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

        ~GraphAttachment() = default;

        bool Init(const drv::ImageView::Descriptor& des);

        bool operator==(const drv::ImageView::Descriptor& des);

    private:
        drv::ImageView::Descriptor descriptor;
        drv::ImagePtr image;
        drv::ImageViewPtr view;
    };

    class GraphFrameBuffer : public RenderGraphResource {
    public:
        GraphFrameBuffer(std::string str)
            : RenderGraphResource(std::move(str))
        {
        }


    };

    using RGResourcePtr = std::unique_ptr<RenderGraphResource>;
    using RGImagePtr = std::unique_ptr<GraphImage>;
    using RGAttachmentPtr = std::unique_ptr<GraphAttachment>;
}