//
// Created by Zach Lee on 2023/4/22.
//

#pragma once

#include <memory>
#include <unordered_map>
#include <perf/ADB.h>
#include <perf/Device.h>

namespace sky::perf {

    struct Widget {
        ~Widget() = default;

        bool opened = true;

        virtual void Render(ADB &adb) = 0;
    };

    struct DeviceWidget : Widget {
        void Render(ADB &adb) override;

        const char* currentDevice = nullptr;
        std::unique_ptr<Device> device;
    };

    struct StartWidget : Widget {
        void Render(ADB &adb) override;

        const char* currentPackage = nullptr;
        const char* currentDevice = nullptr;
        std::vector<std::string> devices;
        std::vector<std::string> packages;

        DeviceWidget* deviceWidget;
    };

    class Profiler {
    public:
        Profiler() = default;
        ~Profiler() = default;

        void Init();
        void Views();
        void Render(ADB &adb);

        std::unordered_map<std::string, std::unique_ptr<Widget>> widgets;
    };

} // namespace sky::perf