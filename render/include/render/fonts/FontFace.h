//
// Created by Zach Lee on 2022/8/19.
//

#pragma once

#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <framework/asset/Asset.h>
#include <render/resources/RenderResource.h>

namespace sky {

    class FontFace : public RenderResource {
    public:
        ~FontFace() = default;

        void SetFace(FT_Face);

    private:
        friend class FontLibrary;
        FT_Face face;
    };

    struct FontAssetData {
        FT_Face face;
    };
    using FontFacePtr = std::shared_ptr<FontFace>;

    namespace impl {
        void LoadFromPath(const std::string& path, FontAssetData& data);
        FontFacePtr CreateFromData(const FontAssetData& data);
    }

    template <>
    struct AssetTraits <FontFace> {
        using DataType = FontAssetData;

        static void LoadFromPath(const std::string& path, DataType& data)
        {
            impl::LoadFromPath(path, data);
        }

        static FontFacePtr CreateFromData(const DataType& data)
        {
            return impl::CreateFromData(data);
        }

        static void SaveToPath(const std::string& path, const DataType& data) {}
    };

    using FontAssetPtr = std::shared_ptr<Asset<FontFace>>;

}