//
// Created by Zach Lee on 2023/2/2.
//

#include <gles/VertexInput.h>
#include <algorithm>

namespace sky::gles {

    bool VertexInput::Init(const Descriptor &desc)
    {
        bindings = desc.bindings;
        attributes.reserve(desc.attributes.size());
        for (auto &attr : desc.attributes) {
            attributes.emplace_back(VertexAttribute {
                attr.location, attr.binding, attr.offset, GetVertexFormat(attr.format)
            });
        }
        std::sort(attributes.begin(), attributes.end(), [](const VertexAttribute &v1, const VertexAttribute &v2) {
            return v1.binding < v2.binding;
        });

        return true;
    }
}
