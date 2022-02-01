//
// Created by Zach Lee on 2022/2/1.
//

#pragma once

#include <framework/environment/Singleton.h>
#include <engine/IService.h>
#include <core/util/Rtti.h>
#include <unordered_map>
#include <memory>

namespace sky {

    class ServiceManager : public Singleton<ServiceManager> {
    public:

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

    private:
        friend class Singleton<ServiceManager>;
        ServiceManager() = default;
        ~ServiceManager() = default;
        using ServicePtr = std::unique_ptr<IService>;

        std::unordered_map<std::string_view, ServicePtr> services;
    };
}
