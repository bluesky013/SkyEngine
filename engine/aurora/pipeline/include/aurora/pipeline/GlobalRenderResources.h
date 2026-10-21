//
// GlobalRenderResources: per-frame global uniform data (time/delta)
// feeding the Global ResourceGroup (set 0).
//

#pragma once

#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/ResourceGroup.h>
#include <aurora/shader/RgBlockDesc.h>

namespace sky::aurora {

    class Device;
    class Shader;

    class GlobalRenderResources {
    public:
        GlobalRenderResources()  = default;
        ~GlobalRenderResources() = default;

        GlobalRenderResources(const GlobalRenderResources &)            = delete;
        GlobalRenderResources &operator=(const GlobalRenderResources &) = delete;

        // globalShader carries the set 0 layout declared in the shared GlobalBlock.h.
        bool Init(Device *device, Shader *globalShader);

        bool UpdateFrame(float time, float deltaTime);

        ResourceGroup *GetGlobalResourceGroup() const
        {
            return mGroup.Get();
        }

        // the canonical global block description (set 0, binding 0)
        static const RgBlockDesc &GetGlobalBlockDesc();

    private:
        Device          *mDevice = nullptr;
        BufferPtr        mUBO;
        ResourceGroupPtr mGroup;
        uint32_t         mUboSize = 0;
    };

} // namespace sky::aurora
