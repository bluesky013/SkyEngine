//
// Created by Zach Lee on 2022/8/19.
//

#include <render/fonts/FontFace.h>
#include <render/fonts/FontLibrary.h>

namespace sky {

//    void FontFace::SetFace(FT_Face value)
//    {
//        face = value;
//    }
//
//    namespace impl {
//        void LoadFromPath(const std::string &path, FontAssetData &data)
//        {
//            FT_New_Face(FontLibrary::Get()->GetLibrary(), path.c_str(), 0, &data.face);
//        }
//
//        FontFacePtr CreateFromData(const FontAssetData &data)
//        {
//            if (data.face == nullptr) {
//                return {};
//            }
//            auto ptr = std::make_shared<FontFace>();
//            ptr->SetFace(data.face);
//            return ptr;
//        }
//    }
}