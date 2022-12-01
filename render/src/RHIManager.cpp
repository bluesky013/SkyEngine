//
// Created by Zach Lee on 2021/12/21.
//

#include <render/RHIManager.h>

namespace sky {

    void RHIManager::ShutDown()
    {
        if (device != nullptr) {
            delete device;
            device = nullptr;
        }
        if (driver != nullptr) {
            vk::Instance::Destroy(driver);
            driver = nullptr;
        }
    }

    vk::Device *RHIManager::GetDevice() const
    {
        return device;
    }

    vk::Instance *RHIManager::GetDriver() const
    {
        return driver;
    }

    bool RHIManager::Initialize(const Descriptor &des)
    {
        if (driver != nullptr) {
            return true;
        }

        vk::Instance::Descriptor drvDes = {};
        drvDes.appName                 = "SkyEngine";
#ifdef _DEBUG
        drvDes.enableDebugLayer = true;
#else
        drvDes.enableDebugLayer = false;
#endif
        drvDes.appName = des.appName;
        driver         = vk::Instance::Create(drvDes);
        if (driver == nullptr) {
            return false;
        }

        vk::Device::Descriptor devDes = {};
        device = driver->CreateDevice(devDes);
        if (device == nullptr) {
            return false;
        }
        return true;
    }

} // namespace sky
