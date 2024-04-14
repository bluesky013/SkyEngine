//
// Created by Zach Lee on 2023/5/9.
//

#include "AndroidPerfManager.h"

namespace sky {

    void ThermalStatusCallback(void *data, AThermalStatus status)
    {
        auto *thermal = reinterpret_cast<AndroidThermal*>(data);
        for (auto &[key, callback] : thermal->callbacks) {
            callback(static_cast<ThermalStatus>(status));
        }
    }

    AndroidThermal::~AndroidThermal()
    {
#if __ANDROID_API__ >= 31
        if (thermalManager != nullptr) {
            AThermal_releaseManager(thermalManager);
            AThermal_unregisterThermalStatusListener(thermalManager, ThermalStatusCallback, this);
        }
#endif
    }

    float AndroidThermal::GetThermalHeadroom() const
    {
#if __ANDROID_API__ >= 31
        return AThermal_getThermalHeadroom(thermalManager, THERMAL_HEADROOM_FORECAST);
#else
        return 0;
#endif
    }

    void AndroidThermal::RegisterStatusChangeCallback(const std::string &key, std::function<void(ThermalStatus)> && callback)
    {
        callbacks.emplace(key, std::move(callback));
    }

    void AndroidThermal::UnregisterStatusChangeCallback(const std::string &key)
    {
        callbacks.erase(key);
    }

    void AndroidPerfManager::Init()
    {
#if __ANDROID_API__ >= 31
        thermal = std::make_unique<AndroidThermal>();
        thermal->thermalManager = AThermal_acquireManager();
        AThermal_registerThermalStatusListener(thermal->thermalManager, ThermalStatusCallback, thermal.get());
#endif

#if __ANDROID_API__ >= 33
        hint = std::make_unique<AndroidPerformanceHint>();
        hint->performanceHintManager = APerformanceHint_getManager();
#endif
    }

    AndroidPerformanceHint::~AndroidPerformanceHint()
    {
        performanceHintManager = nullptr;
    }

    AndroidPerformanceHint::HintSession::~HintSession()
    {
#if __ANDROID_API__ >= 33
        APerformanceHint_closeSession(session);
#endif
    }

    int64_t AndroidPerformanceHint::HintSession::GetPreferredUpdateRateNanos()
    {
#if __ANDROID_API__ >= 33
        return APerformanceHint_getPreferredUpdateRateNanos(manager);
#else
        return 0;
#endif
    }

    int AndroidPerformanceHint::HintSession::UpdateTargetWorkDuration(int64_t targetDurationNanos)
    {
#if __ANDROID_API__ >= 33
        return APerformanceHint_updateTargetWorkDuration(session, targetDurationNanos);
#else
        return 0;
#endif
    }

    int AndroidPerformanceHint::HintSession::ReportActualWorkDuration(int64_t actualDurationNanos)
    {
#if __ANDROID_API__ >= 33
        return APerformanceHint_reportActualWorkDuration(session, actualDurationNanos);
#else
        return 0;
#endif
    }

    std::shared_ptr<IPerformanceHint::Session> AndroidPerformanceHint::CreateSession(const std::vector<int32_t> &threadIds) const
    {
#if __ANDROID_API__ >= 33
        auto hintSession = std::make_shared<HintSession>();
        hintSession->manager = performanceHintManager;
        hintSession->session = APerformanceHint_createSession(performanceHintManager, threadIds.data(), threadIds.size(), DEFAULT_TARGET_NS);
        return hintSession;
#else
        return {};
#endif
    }
} // namespace sky