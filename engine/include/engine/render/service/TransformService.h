//
// Created by Zach Lee on 2022/2/1.
//

#include <engine/IService.h>
#include <engine/render/RenderBufferPool.h>
#include <core/math/Matrix.h>
#include <core/math/Transform.h>
#include <vulkan/Buffer.h>
#include <unordered_map>
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

        void Free(Handle);

        void UpdateTransform(Handle, const Matrix4& trans);

        void OnTick(float time) override;

    private:
        void InitPool();

        uint32_t index = 0;
        std::unique_ptr<RenderBufferPool> pool;
        std::unordered_map<uint32_t, uint32_t> offsetTable;
    };

}