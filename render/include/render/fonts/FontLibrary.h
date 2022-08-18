//
// Created by Zach Lee on 2022/8/19.
//

#pragma once

#include <core/util/Macros.h>
#include <core/environment/Singleton.h>
#include <render/fonts/FontFace.h>
#include <unordered_map>
#include <mutex>

namespace sky {

    class FontLibrary : public Singleton<FontLibrary> {
    public:
        SKY_DISABLE_COPY(FontLibrary)

        void Init();

        FT_Library GetLibrary() const;
    private:
        friend class Singleton<FontLibrary>;

        FontLibrary() = default;

        ~FontLibrary();

        bool isReady = false;
        FT_Library library{};
    };
}