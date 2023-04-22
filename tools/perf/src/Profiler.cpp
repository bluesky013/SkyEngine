//
// Created by Zach Lee on 2023/4/22.
//

#include <perf/Profiler.h>
#include <imgui.h>

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
            deviceWidget->device = nullptr;
            currentDevice = nullptr;
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
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Start Application") && currentDevice != nullptr && currentPackage != nullptr) {
            adb.StartApplication(currentDevice, currentPackage);
        }

        if (ImGui::Button("Device Details") && currentDevice != nullptr) {
            deviceWidget->currentDevice = currentDevice;
            deviceWidget->device = std::make_unique<Device>();
            adb.UpdateDeviceInfo(*deviceWidget->device, currentDevice);
        }

        ImGui::End();
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

    void Profiler::Init()
    {
        auto *device = static_cast<DeviceWidget*>(widgets.emplace("DeviceWidget", std::make_unique<DeviceWidget>()).first->second.get());
        auto *start = static_cast<StartWidget*>(widgets.emplace("StartWidget", std::make_unique<StartWidget>()).first->second.get());

        start->deviceWidget = device;
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