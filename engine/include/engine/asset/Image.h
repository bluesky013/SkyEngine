//
// Created by Zach Lee on 2021/12/5.
//

#pragma once
#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>

namespace sky {

    class ImageAsset : public AssetBase {
    public:
        ImageAsset(const Uuid& id) : AssetBase(id) {}
        ~ImageAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("a4a44abf-1b60-438d-b6c5-1690e548d97c");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

    class ImageHandler : public AssetHandlerBase {
    public:
        ImageHandler() = default;
        ~ImageHandler() = default;

        AssetPtr Create(const Uuid& id);

        AssetPtr Load(const std::string&);
    };

    class Image : public ResourceBase {
    public:
        Image(const Uuid& id) : ResourceBase(id) {}
        ~Image() = default;
    };

    class Texture : public ResourceBase {
    public:
        Texture(const Uuid& id) : ResourceBase(id) {}
        ~Texture() = default;
    };

}