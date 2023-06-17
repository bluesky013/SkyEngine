//
// Created by Zach Lee on 2023/4/22.
//

#include <perf/Module.h>
#include <imgui.h>

namespace sky::perf {

    bool Module::Init()
    {
        adb = std::make_unique<ADB>();
        adb->Init();
        return true;
    }

    void Module::Start()
    {
        gui = std::make_unique<Gui>();
        gui->Init();

        profiler = std::make_unique<Profiler>();
        profiler->Init();
    }

    void Module::Stop()
    {
        gui = nullptr;
    }

    void Module::Tick(float delta)
    {
        gui->Tick(delta, *adb, [this](ADB &adb) {
            profiler->Render(adb);
        });
    }


} // namespace sky::perf