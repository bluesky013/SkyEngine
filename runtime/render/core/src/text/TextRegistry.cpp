//
// Created by blues on 2024/9/13.
//

#include <render/text/TextRegistry.h>

namespace sky {

    FontPtr TextRegistry::LoadFont(const FileSystemPtr &fs, const std::string &name)
    {
        return factory ? factory->LoadFont(fs, name) : nullptr;
    }

    TextPtr TextRegistry::CreateText(const FontPtr &font)
    {
        return factory ? factory->CreateText(font) : nullptr;
    }

    void TextRegistry::Register(Impl* impl)
    {
        factory.reset(impl);
    }

    void TextRegistry::UnRegister()
    {
        factory = nullptr;
    }
} // namespace sky