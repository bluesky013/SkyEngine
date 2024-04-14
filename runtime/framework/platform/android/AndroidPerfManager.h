//
// Created by Zach Lee on 2023/5/9.
//

#pragma once

#include <memory>
#include <unordered_map>
#include <string>

#include <android/thermal.h>
#include <android/performance_hint.h>

#include <framework/performance/AdaptivePerfManager.h>


namespace sky {

    class AndroidThermal : public IThermal {
    public:
        AndroidThermal() = default;
        ~AndroidThermal();

        static constexpr int32_t THERMAL_HEADROOM_FORECAST = 1;

        float GetThermalHeadroom() const override;
        void RegisterStatusChangeCallback(const std::string &key, std::function<void(ThermalStatus)> && callback) override;
        void UnregisterStatusChangeCallback(const std::string &key) override;

        std::unordered_map<std::string, std::function<void(ThermalStatus)>> callbacks;
    private:
        friend class AndroidPerfManager;
        AThermalManager* thermalManager = nullptr;
    };

    class AndroidPerformanceHint : public IPerformanceHint {
    public:
        AndroidPerformanceHint() = default;
        ~AndroidPerformanceHint();

        class HintSession : public Session {
        public:
            HintSession() = default;
            ~HintSession();

            int64_t GetPreferredUpdateRateNanos() override;
            int UpdateTargetWorkDuration(int64_t targetDurationNanos) override;
            int ReportActualWorkDuration(int64_t actualDurationNanos) override;

            APerformanceHintManager* manager = nullptr;
            APerformanceHintSession* session = nullptr;
        };

        static constexpr uint64_t DEFAULT_TARGET_NS = 16666666;

        virtual std::shared_ptr<Session> CreateSession(const std::vector<int32_t> &threadIds) const override;
    private:
        friend class AndroidPerfManager;
        APerformanceHintManager* performanceHintManager = nullptr;
    };

    class AndroidPerfManager : public AdaptivePerfManager {
    public:
        AndroidPerfManager() = default;
        ~AndroidPerfManager() = default;

        void Init();

        IThermal *GetIThermal() const { return thermal.get(); }
        IPerformanceHint *GetPerformanceHint() const { return hint.get(); }

    private:
        std::unique_ptr<AndroidThermal> thermal;
        std::unique_ptr<AndroidPerformanceHint> hint;
    };


}