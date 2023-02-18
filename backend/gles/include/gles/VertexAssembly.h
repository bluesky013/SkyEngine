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
    private:
        GLuint vao = 0;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;
}