//
// Created by Zach Lee on 2023/4/21.
//

#include <stdio.h>

#include <framework/platform/PlatformBase.h>
#include <perf/ADB.h>

using namespace sky;

int main(int argc, char **argv)
{
    Platform::Get()->Init({});

    perf::ADB adb;
    adb.Init();
    auto devices = adb.SearchDevices();
    if (!devices.empty()) {
        adb.EnableWireless(devices[0]);

        perf::Device device;
        adb.UpdateDeviceInfo(device, devices[0]);

        perf::PrintDeviceInfo(device);
    }

    return 0;
}
