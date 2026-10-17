//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/Device.h>
#include <aurora/rhi/SubmissionResult.h>
#include <core/environment/Singleton.h>
#include <core/util/DynamicModule.h>
#include <memory>
#include <mutex>
#include <string>
#include <variant>

namespace sky::aurora {

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
            Impl()          = default;
            virtual ~Impl() = default;

            virtual bool    Init(const Descriptor &) = 0;
            virtual Device *CreateDevice()           = 0;
        };

        void Init(const Descriptor &);

        class DeviceSession {
        public:
            ~DeviceSession();
            Device  *GetDevice() const noexcept;
            uint64_t GetEpoch() const noexcept;
            DeviceSession(const DeviceSession &)            = delete;
            DeviceSession &operator=(const DeviceSession &) = delete;

        private:
            friend class Instance;
            DeviceSession(Instance &, uint64_t);
            Instance &owner;
            uint64_t  epoch;
        };
        // Exclusive against legacy Init and other sessions; destroy context/resources before releasing the session.
        std::variant<GpuError, std::unique_ptr<DeviceSession>> Acquire(const Descriptor &);

        Device *GetDevice() const
        {
            return device;
        }

    private:
        bool                           Load(const Descriptor &);
        void                           Reset();
        std::mutex                     mutex;
        uint64_t                       nextEpoch = 1;
        bool                           claimed   = false;
        Device                        *device    = nullptr;
        std::unique_ptr<Impl>          impl;
        std::unique_ptr<DynamicModule> dynModule;
    };

} // namespace sky::aurora
