//
// Created by Zach Lee on 2026/2/23.
//

#include <pvs/PVSModule.h>
#include <pvs/PVSCulling.h>

namespace sky {

    bool PVSModule::Init(const StartArguments &args)
    {
        binder.Bind(this);
        return true;
    }

    void PVSModule::Start()
    {

    }

    void PVSModule::Shutdown()
    {
        binder.Reset();
    }

    void PVSModule::OnCreateRenderScene(RenderScene* scene)
    {
        pvsCulling = new PVSCulling();
        scene->RegisterCullingSystem(Name("PVS"), pvsCulling);
    }

} // namespace sky