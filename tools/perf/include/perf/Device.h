//
// Created by Zach Lee on 2023/4/21.
//

#pragma once

#include <vector>
#include <string>

namespace sky::perf {

    struct CpuCore {
        int32_t id;
        std::string vendor;
        std::string type;
        std::string frequencies;
        int minFreq;
        int maxFreq;
    };

    struct CpuInfo {
        std::string hardware;
        std::vector<CpuCore> cores;
    };

    struct Device {
        std::string id;
        std::string name;

        std::string cpuTemp1;
        std::string cpuTemp2;
        std::string gpuTemp1;
        std::string gpuTemp2;
        std::string ddrTemp;

        CpuInfo cpu;
    };

    void PrintDeviceInfo(Device &dev);

} // namespace sky::perf