//
// Created by Zach Lee on 2021/12/23.
//

#include <engine/render/service/CameraService.h>
#include <engine/render/camera/CameraComponent.h>

namespace sky {

    CameraService::CameraService()
    {
        ComponentFactory<CameraComponent>::Get()->RegisterListener(this);
    }

    CameraService::~CameraService()
    {
        ComponentFactory<CameraComponent>::Get()->UnRegisterListener(this);
    }

    void CameraService::OnTick(float time)
    {

    }
}