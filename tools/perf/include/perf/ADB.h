//
// Created by Zach Lee on 2023/4/21.
//

#pragma once

#include <perf/Device.h>

namespace sky::perf {

    class ADB {
    public:
        ADB() = default;
        ~ADB() = default;

        bool Init();
        std::vector<std::string> SearchDevices();
        std::vector<std::string> SearchPackages(const std::string &id) const;
        void EnableWireless(const std::string &id) const;
        std::string GetDeviceName(const std::string &id) const;
        void UpdateDeviceInfo(Device &dev, const std::string &id) const;

        std::vector<std::string> Execute(const std::string &cmd) const;

    private:
        void ProcessCPUInfo(Device &dev) const;

        std::string adb;
        std::vector<Device> devices;
    };


} // namespace sky::perf