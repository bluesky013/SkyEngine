//
// Created by blues on 2024/9/11.
//

#include <freetype/FreeTypeLibrary.h>
#include <core/logger/Logger.h>

namespace sky {

    void FreeTypeLibrary::Init()
    {
        FT_Error error = FT_Init_FreeType(&library);
        if (error != 0) {
            LOG_E("FreeType", "Freetype init failed, %d", error);
        }
    }

    FreeTypeLibrary::~FreeTypeLibrary()
    {
        if (library != nullptr) {
            FT_Done_FreeType(library);
            library = nullptr;
        }
    }

} // namespace sky