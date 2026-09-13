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
        Shader *shader = GetPassShader();
        if (shader == nullptr) {
            return; // pass shader not wired yet
        }

        ResourceGroup::Descriptor groupDesc{};
        groupDesc.shader = shader;
        groupDesc.set    = 1;
        mPassResourceGroup = device->CreateResourceGroup(groupDesc);
    }

} // namespace sky::aurora
