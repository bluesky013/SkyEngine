//
// Created by Zach Lee on 2023/4/21.
//

#include <perf/ADB.h>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <framework/platform/PlatformBase.h>
#include <core/util/String.h>

namespace sky::perf {

    std::unordered_map<int, std::string> VENDOR_MAP = {
        {0x41, "ARM"},
        {0x42, "Broadcom"},
        {0x43, "Cavium"},
        {0x44, "DigitalEquipment"},
        {0x48, "HiSilicon"},
        {0x49, "Infineon"},
        {0x4D, "Freescale"},
        {0x4E, "NVIDIA"},
        {0x50, "APM"},
        {0x51, "Qualcomm"},
        {0x56, "Marvell"},
        {0x69, "Intel"},
    };

    std::unordered_map<int, std::string> CPU_MAP = {
        // arm
        {0xd03, "Cortex-a53"},
        {0xd05, "Cortex-A55"},
        {0xd07, "Cortex-a57"},
        {0xd08, "Cortex-a72"},
        {0xd0b, "Cortex-A76"},
        {0xd0d, "Cortex-A77"},
        {0xd41, "Cortex-A78"},
        // qualcomm
        {0xc00, "Falkor"},
        // hisilicon
        {0xd01, "Kunpeng-920"},
    };

    // surface_flinger: shell dumpsys SurfaceFlinger --latency [layer]
    // surface_flinger: shell dumpsys gfxinfo [package] framestats
    // memory: shell dumpsys meminfo [package]
    // cpu-frequency: shell cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq
    // cpu-usage: shell cat /proc/%d/stat

    // start app: shell am start --activity-single-top [package]

    bool ADB::Init()
    {
        std::string out;
        Platform::Get()->RunCmd("where adb", out);

        auto tokens = Split(out, "\r\n");
        if (!tokens.empty()) {
            adb = tokens[0] + " ";
            return true;
        }
        return false;
    }

    std::vector<std::string> ADB::SearchDevices() const
    {
        std::vector<std::string> res;
        auto lines = Execute("devices");
        for (auto &line : lines) {
            auto tokens = Split(line, "\t");
            if (tokens.size() >= 2 && tokens[1] == "device") {
                res.emplace_back(tokens[0]);
            }
        }
        return res;
    }

    void ADB::EnableWireless(const std::string &id) const
    {
        std::string ip;
        {
            std::stringstream ss;
            // love from chat gpt
            ss << "-s " << id << " shell \"ifconfig | grep -A 1 'wlan0' | grep 'inet addr' | cut -d ':' -f 2 | cut -d ' ' -f 1\"";
            auto res = Execute(ss.str());
            if (!res.empty()) {
                ip = res[0];
                printf("ip address. %s\n", res[0].c_str());
            }
        }
        {
            std::stringstream ss;
            ss << " tcpip 5555";
            Execute(ss.str());
        }
        {
            std::stringstream ss;
            ss << " connect " << ip.c_str() << ":5555";
            Execute(ss.str());
        }
    }

    void ADB::StartApplication(const std::string &id, const std::string &package) const
    {
        std::string activity;
        {
            std::stringstream ss;
            ss << "-s " << id << " shell cmd package resolve-activity --brief " << package;
            auto res = Execute(ss.str());
            if (!res.empty()) {
                activity = res[1];
                printf("start activity. %s\n", activity.c_str());
            }
        }
        {
            std::stringstream ss;
            ss << "-s " << id << " shell am start --activity-single-top " << activity;
            Execute(ss.str());
        }
    }

    std::vector<std::string> ADB::SearchPackages(const std::string &id) const
    {
        std::vector<std::string> res;

        std::stringstream ss;
        ss << "-s " << id << " shell pm list packages";
        auto lines = Execute(ss.str());
        for (auto &line : lines) {
            auto tokens = Split(line, ":");
            if (tokens.size() != 2) {
                continue;
            }
            res.emplace_back(tokens[1]);
        }

        std::sort(res.begin(), res.end());
        return res;
    }

    std::vector<std::string> ADB::Execute(const std::string &cmd) const
    {
        std::stringstream ss;
        ss << adb << cmd;

        std::string out;
        Platform::Get()->RunCmd(ss.str(), out);
        return Split(out, "\r\n");
    }

    std::string ADB::GetDeviceName(const std::string &id) const
    {
        std::stringstream ss;
        ss << "-s " << id << " shell getprop ro.product.model";
        auto data = Execute(ss.str());
        return data.empty() ? "" : data[0];
    }

    void ADB::UpdateDeviceInfo(Device &device, const std::string &id) const
    {
        device.id = id;
        device.name = GetDeviceName(id);
        ProcessCPUInfo(device);

        printf("id %s: name %s\n", device.id.c_str(), device.name.c_str());
    }

    void ProcessCpuCoreInfo(CpuCore &core, std::vector<std::string> &tokens)
    {
        auto &key = tokens[0];
        tokens[1].erase(std::remove(tokens[1].begin(), tokens[1].end(), ' '), tokens[1].end());
        auto &value = tokens[1];

        if (key == "CPU implementer") {
            auto vendor = std::stoi(value, 0, 16);
            auto iter = VENDOR_MAP.find(vendor);
            core.vendor = iter != VENDOR_MAP.end() ? iter->second : "UNKNOWN";
        } else if (key == "CPU part") {
            auto part = std::stoi(value, 0, 16);
            auto iter = CPU_MAP.find(part);
            core.type = iter != CPU_MAP.end() ? iter->second : "UNKNOWN";
        }
    }

    void ProcessAvailableFrequencies(const ADB &adb, CpuCore &core)
    {
        // /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies
        std::stringstream ss;
        ss << " shell cat /sys/devices/system/cpu/cpu" << core.id << "/cpufreq/scaling_available_frequencies";

        auto lines = adb.Execute(ss.str());
        if (!lines.empty()) {
            core.frequencies = lines[0];
        }
    }

    void ProcessTemperature(const ADB &adb, Device &dev)
    {
        auto zones = adb.Execute(" shell ls /sys/devices/virtual/thermal/thermal_zone*/temp");
        auto types = adb.Execute(" shell cat /sys/devices/virtual/thermal/thermal_zone*/type");

        for (uint32_t i = 0; i < types.size(); ++i) {
            auto &type = types[i];
            if (type.find("cpuss-0") != std::string::npos) {
                dev.cpuTemp1 = zones[i];
            } else if (type.find("cpuss-1") != std::string::npos) {
                dev.cpuTemp2 = zones[i];
            } else if (type.find("gpuss-0") != std::string::npos) {
                dev.gpuTemp1 = zones[i];
            } else if (type.find("gpuss-1") != std::string::npos) {
                dev.gpuTemp2 = zones[i];
            } else if (type.find("ddr") != std::string::npos) {
                dev.ddrTemp = zones[i];
            }
        }

    }

    void ADB::ProcessCPUInfo(Device &dev) const
    {
        std::stringstream ss;
        ss << "-s " << dev.id << " shell cat /proc/cpuinfo";
        auto lines = Execute(ss.str());

        for (auto &line : lines) {
            auto tokens = Split(line, "\t:");
            if (tokens.size() < 2) {
                continue;
            }
            if (tokens[0] == "processor") {
                dev.cpu.cores.emplace_back();
                auto &back = dev.cpu.cores.back();
                back.id = std::stoi(tokens[1]);
                // process frequency
                ProcessAvailableFrequencies(*this, back);

            } else if (tokens[0] == "Hardware") {
                dev.cpu.hardware = tokens[1];
            } else if (!dev.cpu.cores.empty()) {
                ProcessCpuCoreInfo(dev.cpu.cores.back(), tokens);
            }
        }

        // temperature
        ProcessTemperature(*this, dev);

    }

} // namespace sky::perf