//
// Created by Zach Lee on 2023/2/2.
//

#pragma once

#include <mutex>
#include <thread>
#include <unordered_map>
#include <rhi/VertexAssembly.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class VertexAssembly : public rhi::VertexAssembly, public DevObject {
    public:
        VertexAssembly(Device &dev) : DevObject(dev) {}
        ~VertexAssembly();

        bool Init(const Descriptor &desc);

        GLuint AcquireNativeHandle(uint32_t queueIndex);
        GLenum GetIndexType() const { return indexType; }

    private:
        void InitInternal(GLuint vao);

        // OpenGL-ES explicitly disallows sharing of VAO objects
        std::vector<GLuint> objects;
        GLenum indexType = GL_UNSIGNED_SHORT;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;
}