//
// Created by Zach Lee on 2022/2/1.
//

#pragma once

#include <engine/IService.h>
#include <core/type/Rtti.h>
#include <unordered_map>
#include <memory>

namespace sky {

    class ServiceManager {
    public:
        ServiceManager() = default;
        ~ServiceManager() = default;

        template <typename T>
        T* GetService() const
        {
            auto iter = services.find(TypeInfo<T>::Name());
            return iter == services.end() ? nullptr : static_cast<T*>(iter->second.get());
        }

        template <typename T>
        void RegisterService()
        {
            services.emplace(TypeInfo<T>::Name(), new T());
        }

        template <typename T>
        void UnRegisterService()
        {
            services.erase(TypeInfo<T>::Name());
        }

        void Tick(float time)
        {
            for (auto& service : services) {
                service.second->OnTick(time);
            }
        }

    private:
        using ServicePtr = std::unique_ptr<IService>;
        std::unordered_map<std::string_view, ServicePtr> services;
    };
}
