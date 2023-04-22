//
// Created by Zach Lee on 2023/4/22.
//

#include <perf/Profiler.h>
#include <imgui.h>
#include <implot.h>
#include <core/util/String.h>

namespace sky::perf {

    static bool ArrayGetter(void* data, int idx, const char** out)
    {
        const std::string* items = (const std::string*)data;
        if (out) {
            *out = items[idx].c_str();
        }
        return true;
    }

    void StartWidget::Render(ADB &adb)
    {
        if (!opened) {
            return;
        }

        ImGui::Begin("Start Application", &opened);

        if (ImGui::Button("Search Devices")) {
            devices = adb.SearchDevices();
            currentDevice = nullptr;
            for (auto &widget : widgets) {
                widget->UpdateDevice(adb, currentDevice);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Enable Wireless") && currentDevice != nullptr) {
            adb.EnableWireless(currentDevice);
        }

        if (ImGui::BeginCombo("devices", currentDevice)) {
            for (auto &device : devices) {
                bool selected = (currentDevice == device.c_str());
                if (ImGui::Selectable(device.c_str(), selected)) {
                    currentDevice = device.c_str();
                    for (auto &widget : widgets) {
                        widget->UpdateDevice(adb, currentDevice);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Search Packages") && currentDevice != nullptr) {
            packages = adb.SearchPackages(currentDevice);
            currentPackage = nullptr;
        }

        if (ImGui::BeginCombo("packages", currentPackage)) {
            for (auto &package : packages) {
                bool selected = (currentPackage == package.c_str());
                if (ImGui::Selectable(package.c_str(), selected)) {
                    currentPackage = package.c_str();
                    for (auto &widget : widgets) {
                        widget->currentPackage = currentPackage;
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Device Details") && currentDevice != nullptr) {
            for (auto &widget : widgets) {
                widget->UpdateDeviceDetail(adb);
            }
        }

        if (ImGui::Button("Start Application") && currentDevice != nullptr && currentPackage != nullptr) {
            adb.StartApplication(currentDevice, currentPackage);
        }

        if (ImGui::Button("Start Capture") && currentDevice != nullptr) {
            for (auto &widget : widgets) {
                widget->startCapture = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop Capture") && currentDevice != nullptr) {
            for (auto &widget : widgets) {
                widget->startCapture = false;
            }
        }

        ImGui::End();
    }

    void DeviceWidget::UpdateDeviceDetail(ADB &adb)
    {
        if (!device) {
            device = std::make_unique<Device>();
        }
        if (currentDevice != nullptr) {
            adb.UpdateDeviceInfo(*device, currentDevice);
        }
    }

    void DeviceWidget::Render(ADB &adb)
    {
        if (!opened) {
            return;
        }

        ImGui::Begin("Device Info", &opened);

        if (device) {
            ImGui::Text("Device ID %s", device->name.c_str());
            ImGui::Text("Device Name %s", device->id.c_str());

            ImGui::NewLine();
            ImGui::Text("CPU Info:");
            ImGui::Text("CPU Hardware: %s", device->cpu.hardware.c_str());
            for (auto &core : device->cpu.cores) {
                ImGui::NewLine();
                ImGui::Text("Core: %d", core.id);
                ImGui::Text("Vendor: %s", core.vendor.c_str());
                ImGui::Text("Type: %s", core.type.c_str());
                ImGui::Text("Frequencies: %s", core.frequencies.c_str());
            }
        }
        ImGui::End();
    }

    void FPS::Render(ADB &adb)
    {
        if (!opened) {
            return;
        }

        ImGui::Begin("FPS", &opened);

        if (currentDevice != nullptr && currentPackage != nullptr && startCapture) {
            auto fps = std::stoi(adb.Fps(currentDevice, currentPackage));
            ImGui::Text("fps : %f", static_cast<float>(1000000000.0 / fps));
        }

        ImGui::End();
    }

    void CPUMemoryWidget::Render(ADB &adb)
    {
        if (!opened) {
            return;
        }

        ImGui::Begin("CPU Memory", &opened);

        if (currentDevice != nullptr && currentPackage != nullptr && startCapture) {
            auto lines = adb.Memory(currentDevice, currentPackage);
            for (auto &line : lines) {
                auto tokens = Split(line, ": ");
                if (tokens.size() == 4 && tokens[1] == "Heap") {
                    ImGui::Text("%s : %s", tokens[0].c_str(), tokens[3].c_str());
                } else if (tokens.size() == 3 && tokens[0] == "Graphics") {
                    ImGui::Text("Graphics : %s", tokens[1].c_str());
                }
            }
        }
        ImGui::End();
    }

    void CPUFrequencyWidget::Render(ADB &adb)
    {
        if (!opened) {
            return;
        }

        ImGui::Begin("CPU Frequency", &opened);

        if (currentDevice != nullptr && startCapture) {
            std::vector<std::string> frequencies = adb.Frequencies(currentDevice);
            for (uint32_t i = 0; i < frequencies.size(); ++i) {
                auto &freq = frequencies[i];
                if (freq.empty()) {
                    continue;
                }
                ImGui::Text("cpu %u: freq %s", i, freq.c_str());
            }
        }

        ImGui::End();
    }

    void Profiler::Init()
    {
        auto *device = static_cast<DeviceWidget*>(widgets.emplace("DeviceWidget", std::make_unique<DeviceWidget>()).first->second.get());
        auto *frequency = static_cast<CPUFrequencyWidget*>(widgets.emplace("CPUFrequency", std::make_unique<CPUFrequencyWidget>()).first->second.get());
        auto *memory = static_cast<CPUMemoryWidget*>(widgets.emplace("CPUMemory", std::make_unique<CPUMemoryWidget>()).first->second.get());
        auto *fps = static_cast<FPS*>(widgets.emplace("FPS", std::make_unique<FPS>()).first->second.get());

        auto *start = static_cast<StartWidget*>(widgets.emplace("StartWidget", std::make_unique<StartWidget>()).first->second.get());
        start->widgets.emplace_back(device);
        start->widgets.emplace_back(frequency);
        start->widgets.emplace_back(memory);
        start->widgets.emplace_back(fps);
    }

    void Profiler::Views()
    {
        ImGui::Begin("Views");

        for (auto &[name, widget] : widgets) {
            ImGui::Checkbox(name.c_str(), &widget->opened);
        }

        ImGui::End();
    }

    void Profiler::Render(ADB &adb)
    {
        Views();
        for (auto &widget : widgets) {
            widget.second->Render(adb);
        }
    }

} // namespace sky::perf