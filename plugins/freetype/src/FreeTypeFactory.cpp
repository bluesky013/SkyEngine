//
// Created by blues on 2024/9/13.
//

#include <freetype/FreeTypeFactory.h>
#include <freetype/FreeTypeFont.h>
#include <freetype/FreeTypeText.h>

namespace sky {

    FontPtr FreeTypeFactory::LoadFont(const FileSystemPtr &fs, const std::string &name)
    {
        auto *font = new FreeTypeFont(name);
        auto file = fs->OpenFile(name);
        if (!file) {
            return nullptr;
        }

        font->Load(file);
        return font;
    }

    Text* FreeTypeFactory::CreateText(const FontPtr &font)
    {
        return new FreeTypeText(font);
    }
} // namespace sky