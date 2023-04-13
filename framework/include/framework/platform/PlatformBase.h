//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <cstdint>
#include <string>
#include <core/environment/Singleton.h>

namespace sky {

    class PlatformBase {
    public:
        PlatformBase() = default;
        virtual ~PlatformBase() = default;

        struct Descriptor {
            void* application = nullptr;
        };

        virtual bool Init(const Descriptor&) = 0;

        virtual void Shutdown() = 0;

        virtual uint64_t GetPerformanceFrequency() const = 0;

        virtual uint64_t GetPerformanceCounter() const = 0;

        virtual std::string GetInternalPath() const { return ""; }

        static PlatformBase *GetPlatform();
    };
}
