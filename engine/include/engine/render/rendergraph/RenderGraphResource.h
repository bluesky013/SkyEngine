//
// Created by Zach Lee on 2021/12/22.
//

#pragma once

#include <vulkan/Image.h>
#include <memory>

namespace sky {

    class RenderGraphResource {
    public:
        RenderGraphResource() = default;
        virtual ~RenderGraphResource() = default;
    };

    class GraphImage : public RenderGraphResource {
    public:
        GraphImage() {}
        ~GraphImage() = default;

        void Update(const drv::Image::Descriptor& des);

    private:
        drv::Image::Descriptor descriptor;
        drv::ImagePtr image;
    };

    class GraphAttachment : public RenderGraphResource {
    public:
        GraphAttachment(GraphImage& img) : image(img) {}
        ~GraphAttachment() = default;

        void Update(const drv::Image::Descriptor& des);

    private:
        GraphImage& image;
        drv::ImageView::Descriptor descriptor;
        drv::ImageViewPtr view;
        VkClearValue clear;
    };

    using RGResourcePtr = std::shared_ptr<RenderGraphResource>;
    using RGImagePtr = std::shared_ptr<GraphImage>;
    using RGAttachmentPtr = std::shared_ptr<GraphAttachment>;
}