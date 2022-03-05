//
// Created by Zach Lee on 2022/2/1.
//

#include <engine/render/service/TransformService.h>
#include <engine/render/DriverManager.h>
#include <glm/gtc/matrix_transform.hpp>

namespace sky {

    TransformService::TransformService()
    {
    }

    TransformService::~TransformService()
    {
    }

    TransformService::Handle TransformService::Acquire()
    {
        return bufferPool.Acquire();
    }

    void TransformService::Free(Handle& handle)
    {
        bufferPool.Free(handle);
    }

    void TransformService::OnTick(float time)
    {
        bufferPool.Flush();
    }

    void TransformService::UpdateTransform(const Handle& handle, const Matrix4& trans)
    {
        bufferPool.Update(handle, trans,
                          offsetof(ObjectInfo, worldMatrix));

        bufferPool.Update(handle, glm::mat4(glm::transpose(glm::inverse(glm::mat3(trans)))),
                          offsetof(ObjectInfo, inverseTransMatrix));
    }

}