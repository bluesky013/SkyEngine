//
// Created by Zach Lee on 2023/5/9.
//

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace sky {

    enum class ThermalStatus {
        ERROR = -1,
        NONE = 0,
        LIGHT = 1,
        MODERATE = 2,
        SEVERE = 3,
        CRITICAL = 4,
        EMERGENCY = 5,
        SHUTDOWN = 6,
    };

    class IThermal {
    public:
        virtual float GetThermalHeadroom() const = 0;
        virtual void RegisterStatusChangeCallback(const std::string &key, std::function<void(ThermalStatus)> && callback) = 0;
        virtual void UnregisterStatusChangeCallback(const std::string &key) = 0;

    protected:
        IThermal() = default;
        ~IThermal() = default;
    };

    class IPerformanceHint {
    public:
        struct Session {
        protected:
            Session() = default;
            ~Session() = default;

            virtual int64_t GetPreferredUpdateRateNanos() = 0;
            virtual int UpdateTargetWorkDuration(int64_t targetDurationNanos) = 0;
            virtual int ReportActualWorkDuration(int64_t actualDurationNanos) = 0;
        };

        virtual std::shared_ptr<Session> CreateSession(const std::vector<int32_t> &threadIds) const = 0;

    protected:
        IPerformanceHint() = default;
        ~IPerformanceHint() = default;
    };

    class AdaptivePerfManager {
    public:
        AdaptivePerfManager()  = default;
        virtual ~AdaptivePerfManager() = default;

        virtual void Init() {};

        IThermal *GetIThermal() const { return nullptr; }
        IPerformanceHint *GetPerformanceHint() const { return nullptr; }

    };
} // namespace sky