//
// Created by Zach Lee on 2023/5/9.
//

#include "AndroidPerfManager.h"

namespace sky {

    void ThermalStatusCallback(void *data, AThermalStatus status)
    {
        auto *thermal = reinterpret_cast<AndroidThermal *>(data);
        for (auto &[key, callback] : thermal->callbacks) {
            callback(static_cast<ThermalStatus>(status));
        }
    }

    AndroidThermal::~AndroidThermal()
    {
#if __ANDROID_API__ >= 31
        if (thermalManager != nullptr) {
            AThermal_unregisterThermalStatusListener(thermalManager, ThermalStatusCallback, this);
            AThermal_releaseManager(thermalManager);
        }
#endif
    }

    float AndroidThermal::GetThermalHeadroom() const
    {
#if __ANDROID_API__ >= 31
        return thermalManager != nullptr ? AThermal_getThermalHeadroom(thermalManager, THERMAL_HEADROOM_FORECAST) : 0.f;
#else
        return 0;
#endif
    }

    void AndroidThermal::RegisterStatusChangeCallback(const std::string &key, std::function<void(ThermalStatus)> &&callback)
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
        thermal                 = std::make_unique<AndroidThermal>();
        thermal->thermalManager = AThermal_acquireManager();
        if (thermal->thermalManager != nullptr) {
            AThermal_registerThermalStatusListener(thermal->thermalManager, ThermalStatusCallback, thermal.get());
        }
#endif

#if __ANDROID_API__ >= 33
        hint                         = std::make_unique<AndroidPerformanceHint>();
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
        if (session != nullptr) {
            APerformanceHint_closeSession(session);
        }
#endif
    }

    int64_t AndroidPerformanceHint::HintSession::GetPreferredUpdateRateNanos()
    {
#if __ANDROID_API__ >= 33
        return manager != nullptr ? APerformanceHint_getPreferredUpdateRateNanos(manager) : 0;
#else
        return 0;
#endif
    }

    int AndroidPerformanceHint::HintSession::UpdateTargetWorkDuration(int64_t targetDurationNanos)
    {
#if __ANDROID_API__ >= 33
        return session != nullptr ? APerformanceHint_updateTargetWorkDuration(session, targetDurationNanos) : -1;
#else
        return 0;
#endif
    }

    int AndroidPerformanceHint::HintSession::ReportActualWorkDuration(int64_t actualDurationNanos)
    {
#if __ANDROID_API__ >= 33
        return session != nullptr ? APerformanceHint_reportActualWorkDuration(session, actualDurationNanos) : -1;
#else
        return 0;
#endif
    }

    std::shared_ptr<IPerformanceHint::Session> AndroidPerformanceHint::CreateSession(const std::vector<int32_t> &threadIds) const
    {
#if __ANDROID_API__ >= 33
        if (performanceHintManager == nullptr || threadIds.empty()) {
            return {};
        }
        auto hintSession     = std::make_shared<HintSession>();
        hintSession->manager = performanceHintManager;
        hintSession->session = APerformanceHint_createSession(performanceHintManager, threadIds.data(), threadIds.size(), DEFAULT_TARGET_NS);
        if (hintSession->session == nullptr) {
            return {};
        }
        return hintSession;
#else
        return {};
#endif
    }
} // namespace sky
