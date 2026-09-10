//
// GlobalRenderResources implementation.
//

#include <aurora/pipeline/rg/GlobalRenderResources.h>
#include <aurora/scene/SceneView.h>
#include <aurora/rhi/Device.h>

#include <cstring>

namespace sky::aurora {

    namespace {
        RgBlockDesc MakeGlobalBlockDesc()
        {
            RgBlockDesc desc{};
            desc.set       = 0;
            desc.binding   = 0;
            desc.blockName = Name("Global");
            desc.kind      = RgBlockKind::CBUFFER;
            desc.fields    = {
                {RgFieldType::MAT4, Name("View")},
                {RgFieldType::MAT4, Name("Proj")},
                {RgFieldType::MAT4, Name("ViewProj")},
                {RgFieldType::FLOAT4, Name("CameraPos")},
            };
            return desc;
        }
    } // namespace

    const RgBlockDesc &GlobalRenderResources::GetGlobalBlockDesc()
    {
        static const RgBlockDesc desc = MakeGlobalBlockDesc();
        return desc;
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
        GlobalParams params{};
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
