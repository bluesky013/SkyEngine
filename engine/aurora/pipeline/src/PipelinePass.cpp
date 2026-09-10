//
// PipelinePass implementation.
//

#include <aurora/pipeline/PipelinePass.h>
#include <aurora/rhi/Device.h>

namespace sky::aurora {

    const std::vector<RgBlockDesc> PipelinePass::mEmptyBlocks = {};

    void PipelinePass::OnSetup(Device *device)
    {
        mDevice = device;
        RebuildPassResources(device);
    }

    void PipelinePass::OnSceneChanged()
    {
        if (mDevice != nullptr) {
            RebuildPassResources(mDevice);
        }
    }

    void PipelinePass::RebuildPassResources(Device *device)
    {
        const auto &blocks = GetPassBlocks();
        if (blocks.empty()) {
            return;
        }

        ResourceGroupLayout::Descriptor layoutDesc{};
        for (const auto &block : blocks) {
            const auto single = ToLayoutDescriptor(block);
            for (const auto &binding : single.bindings) {
                layoutDesc.bindings.push_back(binding);
            }
        }

        mPassLayout = device->CreateResourceGroupLayout(layoutDesc);
        if (mPassLayout == nullptr) {
            return;
        }

        ResourceGroup::Descriptor groupDesc{};
        groupDesc.layout = mPassLayout.Get();
        mPassResourceGroup = device->CreateResourceGroup(groupDesc);
    }

} // namespace sky::aurora
