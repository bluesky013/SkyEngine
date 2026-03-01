//
// Created by blues on 2024/9/13.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>
#include <render/text/Font.h>
#include <render/text/Text.h>

namespace sky {
    class TextRegistry : public Singleton<TextRegistry> {
    public:
        TextRegistry() = default;
        ~TextRegistry() override = default;

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual FontPtr LoadFont(const FileSystemPtr &fs, const std::string &name) = 0;
            virtual Text* CreateText(const FontPtr &font) = 0;
        };

        FontPtr LoadFont(const FileSystemPtr &fs, const std::string &name);
        Text* CreateText(const FontPtr &font);

        void Register(Impl* factory);
        void UnRegister();

    private:
        std::unique_ptr<Impl> factory;
    };

} // namespace sky
