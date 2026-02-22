//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/Buffer.h>
#include <rhi/VertexInput.h>
#include <memory>
#include <vector>

namespace sky::rhi {

    class VertexAssembly {
    public:
        VertexAssembly() = default;
        virtual ~VertexAssembly() = default;

        struct Descriptor {
            std::vector<BufferView> vertexBuffers;
            VertexInputPtr vertexInput;
        };

    protected:
        Descriptor descriptor;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;
}
