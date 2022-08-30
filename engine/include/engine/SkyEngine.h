//
// Created by Zach Lee on 2021/11/12.
//

#pragma once
#include <core/environment/Singleton.h>
#include <framework/interface/IEngine.h>
#include <framework/window/IWindowEvent.h>
#include <memory>
#include <vector>

namespace sky {

    struct IEngineEvent {
        virtual void OnTick(float time)
        {
        }

        virtual void OnWindowResize(void *hwnd, uint32_t, uint32_t)
        {
        }
    };

    class SkyEngine : public IEngine, public Singleton<SkyEngine> {
    public:
        virtual bool Init(const StartInfo &) override;

        virtual void Tick(float) override;

        virtual void DeInit() override;

        static void Reflect();

    private:
        template <typename T>
        friend class Singleton;

        SkyEngine()  = default;
        ~SkyEngine() = default;
    };

} // namespace sky