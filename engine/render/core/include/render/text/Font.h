//
// Created by blues on 2024/9/11.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/file/FileSystem.h>

namespace sky {

    class Font : public RefObject {
    public:
        explicit Font(const std::string &name) : fontName(name) {} // NOLINT
        ~Font() override = default;

        virtual void Load(const FilePtr &file) = 0;
        virtual bool IsReady() const = 0;

    protected:
        std::string fontName;
    };

    using FontPtr = CounterPtr<Font>;

} // namespace sky
