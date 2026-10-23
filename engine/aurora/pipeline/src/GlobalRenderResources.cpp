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

        std::string error;
        mUploadPlan = globalShader->GetUniformPlan(0, 0, shared::GlobalParams::Schema(), error);
        if (!mUploadPlan)
            return false;
        mUboSize = mUploadPlan->targetSize;

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
        return encoder && encoder->WriteBuffer(0, mUBO.Get(), 0, mUboSize) && encoder->End();
    }

    bool GlobalRenderResources::UpdateFrame(float time, float deltaTime)
    {
        if (!mUBO || !std::isfinite(time) || !std::isfinite(deltaTime) || deltaTime < 0)
            return false;
        const shared::GlobalParams params{{time, deltaTime, 0, 0}};
        std::vector<std::byte>     bytes;
        std::string                error;
        if (!mUploadPlan || !EncodeShaderUniform(*mUploadPlan, std::as_bytes(std::span{&params, 1}), bytes, error))
            return false;
        auto *mapped = mUBO->Map();
        if (!mapped)
            return false;
        std::memcpy(mapped, bytes.data(), bytes.size());
        mUBO->UnMap();
        return true;
    }
} // namespace sky::aurora
