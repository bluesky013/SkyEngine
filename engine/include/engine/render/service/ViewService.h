//
// Created by Zach Lee on 2022/2/2.
//

#include <engine/IService.h>
#include <engine/render/RenderBufferPool.h>
#include <core/math/Transform.h>
#include <core/math/Matrix.h>

namespace sky {

    struct ViewData {
        Matrix4 viewMatrix;
        Matrix4 viewProjectMatrix;
    };

    class ViewService : public IService {
    public:
        ViewService();
        ~ViewService();

        void OnTick(float time);

    private:
        void InitPool();

        uint32_t index = 0;
        std::unique_ptr<RenderBufferPool> pool;
    };

}