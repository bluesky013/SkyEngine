//
// Created by Zach Lee on 2023/9/9.
//

#include <render/VertexDescLibrary.h>
#include <render/RHI.h>

namespace sky {

    void VertexDescLibrary::RegisterVertexDesc(const std::string &key, const rhi::VertexInputPtr &desc)
    {
        descriptions.emplace(key, desc);
    }

    rhi::VertexInputPtr VertexDescLibrary::FindVertexDesc(const std::string &key) const
    {
        auto iter = descriptions.find(key);
        return iter != descriptions.end() ? iter->second : nullptr;
    }

    VertexDescLibPtr CreateBuiltinVertexLibrary()
    {
        auto *device = RHI::Get()->GetDevice();

        auto *vtxLib = new VertexDescLibrary();
        {
            rhi::VertexInput::Descriptor vtxDesc = {};
            auto empty = device->CreateVertexInput(vtxDesc);
            vtxLib->RegisterVertexDesc("empty", empty);
        }
        {
            rhi::VertexInput::Descriptor vtxDesc = {};
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 0, 0, 0, rhi::Format::F_RGBA32});
            vtxDesc.bindings.emplace_back(rhi::VertexBindingDesc{0, 16, rhi::VertexInputRate::PER_VERTEX});
            auto posOnly = device->CreateVertexInput(vtxDesc);
            vtxLib->RegisterVertexDesc("position_only", posOnly);
        }
        {
            rhi::VertexInput::Descriptor vtxDesc = {};
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 0, 0, 0, rhi::Format::F_RGBA32});
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 1, 1, 0, rhi::Format::F_RG32});
            vtxDesc.bindings.emplace_back(rhi::VertexBindingDesc{0, 16, rhi::VertexInputRate::PER_VERTEX});
            vtxDesc.bindings.emplace_back(rhi::VertexBindingDesc{1, 8, rhi::VertexInputRate::PER_VERTEX});
            auto unlit = device->CreateVertexInput(vtxDesc);
            vtxLib->RegisterVertexDesc("unlit", unlit);
        }
        {
            rhi::VertexInput::Descriptor vtxDesc = {};
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 0, 0, 0, rhi::Format::F_RGBA32});
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 1, 1, 0, rhi::Format::F_RGBA32});
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 2, 1, 16, rhi::Format::F_RGBA32});
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 3, 1, 32, rhi::Format::F_RGBA32});
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 4, 1, 48, rhi::Format::F_RGBA32});
            vtxDesc.bindings.emplace_back(rhi::VertexBindingDesc{0, 16, rhi::VertexInputRate::PER_VERTEX});
            vtxDesc.bindings.emplace_back(rhi::VertexBindingDesc{1, 64, rhi::VertexInputRate::PER_VERTEX});
            auto standard = device->CreateVertexInput(vtxDesc);
            vtxLib->RegisterVertexDesc("standard", standard);
        }
        {
            rhi::VertexInput::Descriptor vtxDesc = {};
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 0, 0, 0, rhi::Format::F_RGBA32});
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 1, 0, 16, rhi::Format::F_RGBA32});
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 2, 0, 32, rhi::Format::F_RGBA32});
            vtxDesc.bindings.emplace_back(rhi::VertexBindingDesc{0, 48, rhi::VertexInputRate::PER_VERTEX});
            auto geometry = device->CreateVertexInput(vtxDesc);
            vtxLib->RegisterVertexDesc("geometry", geometry);
        }
        {
            rhi::VertexInput::Descriptor vtxDesc = {};
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 0, 0, 0, rhi::Format::F_RG32});
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 1, 0, 8, rhi::Format::F_RG32});
            vtxDesc.attributes.emplace_back(rhi::VertexAttributeDesc{"", 2, 0, 16, rhi::Format::F_RGBA8});
            vtxDesc.bindings.emplace_back(rhi::VertexBindingDesc{0, 20, rhi::VertexInputRate::PER_VERTEX});
            auto gui = device->CreateVertexInput(vtxDesc);
            vtxLib->RegisterVertexDesc("gui", gui);
        }

        return vtxLib;
    }

} // namespace sky
