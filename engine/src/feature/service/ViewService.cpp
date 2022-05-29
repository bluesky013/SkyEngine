//
// Created by Zach Lee on 2021/12/23.
//

#include <engine/feature/service/ViewService.h>
#include <engine/feature/camera/CameraComponent.h>

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

    void ViewService::UpdateViewInfo(const Handle& handle, const Matrix4& view, const Matrix4& project)
    {
        viewPool.Update(handle, view, offsetof(ViewData, worldToView));
        viewPool.Update(handle,  project * view, offsetof(ViewData, worldToClip));
    }
}