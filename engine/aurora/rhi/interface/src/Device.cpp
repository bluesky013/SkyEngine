//
// Created by Zach Lee on 2026/3/29.
//

#include <aurora/rhi/Device.h>

namespace sky::aurora {

    void Device::Shutdown()
    {
        WaitIdle();
    }

    bool Device::Init()
    {
        DeviceInit devInit = {};
        if (!OnInit(devInit)) {
            return false;
        }

        UpdateDeviceCaps();

        return true;
    }

} // namespace sky::aurora