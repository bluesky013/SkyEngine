//
// Created by Zach Lee on 2021/12/26.
//

#pragma once

#include <engine/render/rendergraph/RenderGraphResource.h>
#include <engine/render/rendergraph/RenderGraphBuilder.h>
#include <vulkan/Image.h>
#include <unordered_map>
#include <list>
#include <set>
#include <string>

namespace sky {

    class RenderView;

    struct GraphOutput {
        uint32_t width;
        uint32_t height;
        VkFormat format;
    };

    class RenderGraphTemplate {
    public:
        RenderGraphTemplate() = default;
        ~RenderGraphTemplate() = default;

        virtual bool HasViewTags(const std::string&) const { return false; }

        virtual void PreparePipeline(RenderGraphBuilder&, std::list<RenderView*>&) {}

        virtual void SetOutputConfig(const GraphOutput&) {}

    protected:
        void RegisterImage(const std::string& viewTag, const std::string& name);

        void RegisterAttachment(const std::string& source, const std::string& name);

        using ImageList = std::list<RGImagePtr>;
        std::unordered_map<std::string, ImageList> viewGroups;
        std::unordered_map<std::string, RGImagePtr> images;
        std::unordered_map<std::string, RGAttachmentPtr> attachments;
    };

}