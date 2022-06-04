//
// Created by Zach Lee on 2021/12/21.
//

#include <render/DriverManager.h>

namespace sky {

    void DriverManager::ShutDown()
    {
        if (device != nullptr) {
            delete device;
            device = nullptr;
        }
        if (driver != nullptr) {
            drv::Driver::Destroy(driver);
            driver = nullptr;
        }
    }

    drv::Device* DriverManager::GetDevice() const
    {
        return device;
    }

    drv::Driver* DriverManager::GetDriver() const
    {
        return driver;
    }

    bool DriverManager::Initialize(const Descriptor& des)
    {
        if (driver != nullptr) {
            return true;
        }

        drv::Driver::Descriptor drvDes = {};
        drvDes.appName = "SkyEngine";
        drvDes.enableDebugLayer = true;
        drvDes.appName = des.appName;
        driver = drv::Driver::Create(drvDes);
        if (driver == nullptr) {
            return false;
        }

        drv::Device::Descriptor devDes = {};
        device = driver->CreateDevice(devDes);
        if (device == nullptr) {
            return false;
        }
        return true;
    }

}