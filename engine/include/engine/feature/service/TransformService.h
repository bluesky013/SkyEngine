//
// Created by Zach Lee on 2022/2/1.
//

#include <engine/IService.h>
#include <engine/render/RenderBufferPool.h>
#include <engine/render/BufferTemplate.h>
#include <core/math/Matrix.h>
#include <core/math/Transform.h>
#include <vulkan/Buffer.h>
#include <list>
#include <vector>
#include <memory>

namespace sky {

    struct ObjectInfo {
        Matrix4 worldMatrix;
        Matrix4 inverseTransMatrix;
    };

    class TransformService : public IService {
    public:
        TransformService();
        ~TransformService();

        using Handle = SHandle<TransformService>;

        Handle Acquire();

        void Free(Handle&);

        void UpdateTransform(const Handle&, const Matrix4& trans);

        void OnTick(float time) override;

    private:
        BufferTemplate<ObjectInfo, Handle> bufferPool;
    };

}