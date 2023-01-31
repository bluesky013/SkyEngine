//
// Created by Zach on 2023/1/31.
//

#pragma once
#include <memory>

namespace sky::rhi {

    class VertexAssembly {
    public:
        VertexAssembly() = default;
        virtual ~VertexAssembly() = default;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;
}
