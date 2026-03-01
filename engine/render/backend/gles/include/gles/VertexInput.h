//
// Created by Zach Lee on 2023/2/2.
//

#pragma once

#include <rhi/VertexInput.h>
#include <gles/Forward.h>
#include <gles/Conversion.h>

namespace sky::gles {
    class Device;

    struct VertexAttribute {
        uint32_t  location = 0;
        uint32_t  binding  = 0;
        uint32_t  offset   = 0;
        VertexFormat format;
    };

    class VertexInput : public rhi::VertexInput {
    public:
        VertexInput()  = default;
        ~VertexInput() = default;

        bool Init(const Descriptor &desc);

        const std::vector<VertexAttribute> &GetAttributes() const {  return attributes; }
        const std::vector<rhi::VertexBindingDesc> &GetBindings() const { return bindings; }

    private:
        std::vector<VertexAttribute> attributes;
        std::vector<rhi::VertexBindingDesc> bindings;
    };

}