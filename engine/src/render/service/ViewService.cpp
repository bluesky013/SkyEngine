//
// Created by Zach Lee on 2021/12/23.
//

#include <engine/render/service/ViewService.h>
#include <engine/render/camera/CameraComponent.h>

namespace sky {

    ViewService::ViewService()
    {
    }

    ViewService::~ViewService()
    {
    }

    ViewService::Handle ViewService::Acquire()
    {
        return viewPool.Acquire();
    }

    void ViewService::Free(Handle& handle)
    {
        return viewPool.Free(handle);
    }

    void ViewService::OnTick(float time)
    {
        viewPool.Flush();
    }
}