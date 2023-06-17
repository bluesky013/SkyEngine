//
// Created by Zach Lee on 2023/4/21.
//

#include <perf/Device.h>
#include <core/logger/Logger.h>

namespace sky::perf {
    namespace {
        const char *TAG = "Perf";
    }

    void PrintDeviceInfo(Device &dev)
    {
        LOG_I(TAG, "Device Info %s, ID[%s]", dev.name.c_str(), dev.id.c_str());

        LOG_I(TAG, "Device CPU Info..........");
        LOG_I(TAG, "CPU hardware : %s", dev.cpu.hardware.c_str());
        for (auto &core : dev.cpu.cores) {
            LOG_I(TAG, "**********CPU Core %d**********", core.id);
            LOG_I(TAG, "Vendor: %s", core.vendor.c_str());
            LOG_I(TAG, "Type: %s", core.type.c_str());
            LOG_I(TAG, "Frequencies: %s", core.frequencies.c_str());
        }
    }

} // namespace sky::pef
