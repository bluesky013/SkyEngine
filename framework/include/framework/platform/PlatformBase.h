//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

namespace sky {

    class PlatformBase {
    public:
        PlatformBase() = default;
        virtual ~PlatformBase() = default;

        virtual bool Init() = 0;

        virtual void Shutdown() = 0;

        virtual uint64_t GetPerformanceFrequency() const = 0;

        virtual uint64_t GetPerformanceCounter() const = 0;

        static PlatformBase *GetPlatform();
    };
}