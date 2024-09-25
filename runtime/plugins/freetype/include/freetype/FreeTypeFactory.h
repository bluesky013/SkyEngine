//
// Created by blues on 2024/9/13.
//

#pragma once

#include <render/text/TextRegistry.h>

namespace sky {

    class FreeTypeFactory : public TextRegistry::Impl {
    public:
        FreeTypeFactory() = default;
        ~FreeTypeFactory() override = default;

        FontPtr LoadFont(const FileSystemPtr &fs, const std::string &name) override;
        Text* CreateText(const FontPtr &font) override;
    };

} // namespace sky