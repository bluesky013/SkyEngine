//
// GlobalRenderResources implementation.
//

#include <aurora/pipeline/GlobalRenderResources.h>
#include <aurora/rhi/Device.h>
#include <cmath>

#include "GlobalBlock.h"

#include <cstring>

namespace sky::aurora {

    const RgBlockDesc &GlobalRenderResources::GetGlobalBlockDesc()
    {
        return shared::GetGlobalParamsBlockDesc();
    }

    bool GlobalRenderResources::Init(Device *device, Shader *globalShader)
    {
        if (!device || !globalShader || globalShader->GetDevice() != device)
            return false;
        mDevice = device;

        mUboSize = sizeof(shared::GlobalParams);

        Buffer::Descriptor bufDesc{};
        bufDesc.size   = mUboSize;
        bufDesc.usage  = BufferUsageFlagBit::UNIFORM;
        bufDesc.memory = MemoryType::CPU_TO_GPU;
        mUBO           = mDevice->CreateBuffer(bufDesc);
        if (mUBO == nullptr) {
            return false;
        }

        if (globalShader == nullptr) {
            return false;
        }

        ResourceGroup::Descriptor groupDesc{};
        groupDesc.shader = globalShader;
        groupDesc.set    = 0;
        mGroup           = mDevice->CreateResourceGroup(groupDesc);
        if (mGroup == nullptr) {
            return false;
        }

        auto encoder = mGroup->CreateEncoder();
        encoder->WriteBuffer(0, mUBO.Get(), 0, mUboSize);
        encoder->End();
        return true;
    }

    bool GlobalRenderResources::UpdateFrame(float time, float deltaTime)
    {
        if (!mUBO || !std::isfinite(time) || !std::isfinite(deltaTime) || deltaTime < 0)
            return false;
        const shared::GlobalParams params{{time, deltaTime, 0, 0}};
        auto                      *mapped = mUBO->Map();
        if (!mapped)
            return false;
        std::memcpy(mapped, &params, sizeof(params));
        return true;
    }
} // namespace sky::aurora
