//
// Created by Zach Lee on 2021/12/21.
//

#pragma once
#include <core/environment/Singleton.h>
#include <vulkan/Device.h>
#include <vulkan/Instance.h>

namespace sky {

    class RHIManager : public Singleton<RHIManager> {
    public:
        struct Descriptor {
            std::string appName;
        };

        bool Initialize(const Descriptor &);

        void ShutDown();

        vk::Device *GetDevice() const;

        vk::Instance *GetDriver() const;

        template <typename T>
        std::shared_ptr<T> CreateDeviceObject(const typename T::Descriptor &des)
        {
            return device->CreateDeviceObject<T>(des);
        }

    private:
        friend class Singleton<RHIManager>;
        RHIManager()     = default;
        ~RHIManager()    = default;
        vk::Instance *driver = nullptr;
        vk::Device *device = nullptr;
    };

} // namespace sky
