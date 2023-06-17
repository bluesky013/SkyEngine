//
// Created by Zach Lee on 2023/4/22.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/window/IWindowEvent.h>
#include <perf/gui/Gui.h>
#include <perf/ADB.h>
#include <perf/Profiler.h>

namespace sky::perf {

    class Module : public IModule, IWindowEvent {
    public:
        Module() = default;
        ~Module() = default;

        bool Init() override;
        void Start() override;
        void Stop() override;
        void Tick(float delta) override;

    private:
        std::unique_ptr<ADB> adb;
        std::unique_ptr<Gui> gui;
        std::unique_ptr<Profiler> profiler;
    };

} // namespace sky::perf