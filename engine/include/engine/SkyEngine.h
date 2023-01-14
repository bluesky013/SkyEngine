//
// Created by Zach Lee on 2021/11/12.
//

#pragma once
#include <core/environment/Singleton.h>
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

    class SkyEngine : public Singleton<SkyEngine> {
    public:
        bool Init();

        void Tick(float);

        void DeInit();

        static void Reflect();

    private:
        template <typename T>
        friend class Singleton;

        SkyEngine()  = default;
        ~SkyEngine() = default;
    };

} // namespace sky