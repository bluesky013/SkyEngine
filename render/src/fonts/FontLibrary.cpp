//
// Created by Zach Lee on 2022/8/19.
//

#include <render/fonts/FontLibrary.h>

namespace sky {

    void FontLibrary::Init()
    {
        isReady = !FT_Init_FreeType( &library );
    }

    FontLibrary::~FontLibrary()
    {
        FT_Done_FreeType(library);
    }

    FT_Library FontLibrary::GetLibrary() const
    {
        return library;
    }
}