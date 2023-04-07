//
// Created by Zach Lee on 2023/2/2.
//

#pragma once

#include <rhi/VertexAssembly.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class VertexAssembly : public rhi::VertexAssembly, public DevObject {
    public:
        VertexAssembly(Device &dev) : DevObject(dev) {}
        ~VertexAssembly();

        bool Init(const Descriptor &desc);

        GLuint GetNativeHandle() const { return vao; }
        bool IsInited() const { return inited; }
        void InitInternal();

    private:
        // OpenGL-ES explicitly disallows sharing of VAO objects

        GLuint vao = 0;
        bool inited = false;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;
}