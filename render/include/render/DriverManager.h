//
// Created by Zach Lee on 2021/12/21.
//

#pragma once
#include <core/environment/Singleton.h>
#include <vulkan/Driver.h>
#include <vulkan/Device.h>

namespace sky {

    class DriverManager : public Singleton<DriverManager> {
    public:
        struct Descriptor {
            std::string appName;
        };

        bool Initialize(const Descriptor&);

        void ShutDown();

        drv::Device* GetDevice() const;

        drv::Driver* GetDriver() const;

        template <typename T>
        std::shared_ptr<T> CreateDeviceObject(const typename T::Descriptor& des)
        {
            return device->CreateDeviceObject<T>(des);
        }

    private:
        friend class Singleton<DriverManager>;
        DriverManager() = default;
        ~DriverManager() = default;
        drv::Driver* driver = nullptr;
        drv::Device* device = nullptr;
    };

}