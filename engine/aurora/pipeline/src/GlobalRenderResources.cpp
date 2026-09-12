//
// GlobalRenderResources implementation.
//

#include <aurora/pipeline/GlobalRenderResources.h>
#include <aurora/scene/SceneView.h>
#include <aurora/rhi/Device.h>

#include <GlobalBlock.gen.h>

#include <cstring>

namespace sky::aurora {

    const RgBlockDesc &GlobalRenderResources::GetGlobalBlockDesc()
    {
        return generated::GetGlobalParamsBlockDesc();
    }

    bool GlobalRenderResources::Init(Device *device)
    {
        mDevice = device;

        uint32_t totalSize = 0;
        ComputeFieldOffsets(GetGlobalBlockDesc(), totalSize);
        mUboSize = totalSize;

        Buffer::Descriptor bufDesc{};
        bufDesc.size   = mUboSize;
        bufDesc.usage  = BufferUsageFlagBit::UNIFORM;
        bufDesc.memory = MemoryType::CPU_TO_GPU;
        mUBO = mDevice->CreateBuffer(bufDesc);
        if (mUBO == nullptr) {
            return false;
        }

        auto *layout = mDevice->CreateResourceGroupLayout(ToLayoutDescriptor(GetGlobalBlockDesc()));
        if (layout == nullptr) {
            return false;
        }

        ResourceGroup::Descriptor groupDesc{};
        groupDesc.layout = layout;
        mGroup = mDevice->CreateResourceGroup(groupDesc);
        if (mGroup == nullptr) {
            return false;
        }

        ResourceUpdateInfo write{};
        write.binding     = 0;
        write.kind        = ResourceWriteKind::BUFFER;
        write.buffer      = mUBO.Get();
        write.bufferRange = mUboSize;
        mGroup->Update({write});
        return true;
    }

    void GlobalRenderResources::UpdateView(const SceneView &view, float time)
    {
        if (mUBO == nullptr) {
            return;
        }
        generated::GlobalParams params{};
        params.view     = view.GetViewMatrix();
        params.proj     = Matrix4::Identity(); // projection matrix accessor lands with camera data
        params.viewProj = view.GetViewProjectMatrix();
        params.cameraPos = Vector4(0.f, 0.f, 0.f, time); // camera position accessor lands later

        uint8_t *mapped = mUBO->Map();
        if (mapped != nullptr) {
            std::memcpy(mapped, &params, mUboSize);
        }
    }

} // namespace sky::aurora
