//
// Created by Zach Lee on 2022/2/2.
//

#pragma once
#include <engine/IService.h>
#include <engine/render/BufferTemplate.h>
#include <core/math/Transform.h>
#include <core/math/Matrix.h>

namespace sky {

    struct ViewData {
        Matrix4 worldToView;
        Matrix4 worldToClip;
    };

    class ViewService : public IService {
    public:
        ViewService();
        ~ViewService();

        using Handle = SHandle<ViewService>;

        Handle Acquire();

        void Free(Handle&);

        void OnTick(float time);

        void UpdateViewInfo(const Handle& handle, const Matrix4& view, const Matrix4& project);

    private:
        BufferTemplate<ViewData, Handle> viewPool;
    };

}