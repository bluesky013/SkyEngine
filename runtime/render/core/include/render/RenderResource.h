//
// Created by blues on 2024/7/24.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <rhi/Stream.h>
#include <string>

namespace sky {

    class RenderResource : public RefObject {
    public:
        RenderResource() = default;
        explicit RenderResource(const std::string &name_) : name(name_) {} // NOLINT
        ~RenderResource() override = default;

        void SetName(const std::string &name_) { name = name_; }
        const std::string &GetName() const { return name; }
    protected:
        std::string name;
    };

} // namespace sky