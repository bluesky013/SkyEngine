//
// Created by Zach Lee on 2022/2/2.
//

#include <engine/IService.h>
#include <engine/render/service/BufferTemplate.h>
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

    private:
        BufferTemplate<ViewData, Handle> viewPool;
    };

}