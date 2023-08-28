//
// Created by Zach Lee on 2023/8/28.
//

#include <render/RenderResourceGroup.h>
#include <render/RHI.h>

namespace sky {

    ResourceGroupLayoutBuilder::ResourceGroupLayoutBuilder()
    {
        layout.reset(new ResourceGroupLayout());
    }

    ResourceGroupLayoutBuilder &ResourceGroupLayoutBuilder::AddDescriptorType(const std::string &name, rhi::DescriptorType type, rhi::ShaderStageFlags visibility, uint32_t binding, uint32_t count)
    {
        layoutDesc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding{
            type, count, binding, visibility, name
        });
        layout->handlers.emplace(name, ResourceGroupLayout::Handler{binding, 0, 0});
        return *this;
    }

    ResourceGroupLayoutBuilder &ResourceGroupLayoutBuilder::AddBufferParamsNames(const std::string &name, uint32_t binding, uint32_t size, uint32_t offset)
    {
        layout->handlers.emplace(name, ResourceGroupLayout::Handler{binding, offset, size});
        return *this;
    }

    ResourceGroupLayout *ResourceGroupLayoutBuilder::Build()
    {
        auto *device = RHI::Get()->GetDevice();
        layout->layout = device->CreateDescriptorSetLayout(layoutDesc);
        if (!layout->layout) {
            return nullptr;
        }
        return layout.release();
    }
} // namespace sky