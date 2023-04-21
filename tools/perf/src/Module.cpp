//
// Created by Zach Lee on 2023/4/22.
//

#include <perf/Module.h>

namespace sky::perf {

    bool Module::Init()
    {
        adb = std::make_unique<ADB>();
        adb->Init();
//        auto devices = adb.SearchDevices();
//        if (!devices.empty()) {
//            adb.EnableWireless(devices[0]);
//
//            perf::Device device;
//            adb.UpdateDeviceInfo(device, devices[0]);
//
//            perf::PrintDeviceInfo(device);
//        }

        return true;
    }

    void Module::Start()
    {
        gui = std::make_unique<Gui>();
        gui->Init();
    }

    void Module::Stop()
    {
        gui = nullptr;
    }

    void Module::Tick(float delta)
    {
        gui->Tick(delta);
    }


} // namespace sky::perf