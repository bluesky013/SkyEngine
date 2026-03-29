//
// Created by blues on 2026/3/29.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/DynamicModule.h>
#include <aurora/rhi/Device.h>
#include <string>
#include <memory>

namespace sky::aurora {

    enum class API {
        DEFAULT = 0,
        VULKAN,
        METAL,
        DX12,
        GLES
    };

    class Instance : public Singleton<Instance> {
    public:
        Instance() = default;
        ~Instance() override;

        struct Descriptor {
            std::string appName;
            std::string engineName;
            bool        enableDebugLayer;
            API         api;
        };

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual bool Init(const Descriptor &) = 0;
            virtual Device *CreateDevice() = 0;
        };

        void Init(const Descriptor &);

        Device* GetDevice() const { return device; }
    private:
        Device* device = nullptr;
        std::unique_ptr<Impl> impl;
        std::unique_ptr<DynamicModule> dynModule;
    };

} // namespace sky::aurora
