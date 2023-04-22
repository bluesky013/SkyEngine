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
        bool startCapture = false;
        const char* currentDevice = nullptr;
        const char* currentPackage = nullptr;

        virtual void UpdateDevice(ADB &adb, const char *dev) { currentDevice = dev; }
        virtual void UpdateDeviceDetail(ADB &adb) {}
        virtual void Render(ADB &adb) = 0;
    };

    struct FPS : Widget {
        void Render(ADB &adb) override;

        const char* currentSurfaceView = nullptr;
    };

    struct CPUMemoryWidget : Widget {
        void Render(ADB &adb) override;
    };

    struct CPUFrequencyWidget : Widget {
        void Render(ADB &adb) override;
    };

    struct DeviceWidget : Widget {
        void UpdateDeviceDetail(ADB &adb) override;
        void Render(ADB &adb) override;

        std::unique_ptr<Device> device;
    };

    struct StartWidget : Widget {
        void Render(ADB &adb) override;

        std::vector<std::string> devices;
        std::vector<std::string> packages;

        std::vector<Widget*> widgets;
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