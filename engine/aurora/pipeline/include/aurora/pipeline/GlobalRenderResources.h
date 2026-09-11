//
// GlobalRenderResources: per-frame global uniform data (view/proj/time)
// feeding the Global ResourceGroup (set 0).
//

#pragma once

#include <aurora/shader/RgBlockDesc.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/ResourceGroup.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector4.h>

namespace sky::aurora {

    class Device;
    class SceneView;

    // must match the generated Global cbuffer layout
    struct GlobalParams {
        Matrix4 view;
        Matrix4 proj;
        Matrix4 viewProj;
        Vector4 cameraPos; // xyz + time in w
    };

    class GlobalRenderResources {
    public:
        GlobalRenderResources() = default;
        ~GlobalRenderResources() = default;

        GlobalRenderResources(const GlobalRenderResources &) = delete;
        GlobalRenderResources &operator=(const GlobalRenderResources &) = delete;

        bool Init(Device *device);

        // per-frame: write UBO from view + time, then RG.Update
        void UpdateView(const SceneView &view, float time);

        ResourceGroup *GetGlobalResourceGroup() const { return mGroup.Get(); }

        // the canonical global block description (set 0, binding 0)
        static const RgBlockDesc &GetGlobalBlockDesc();

    private:
        Device          *mDevice = nullptr;
        BufferPtr        mUBO;
        ResourceGroupPtr mGroup;
        uint32_t         mUboSize = 0;
    };

} // namespace sky::aurora
