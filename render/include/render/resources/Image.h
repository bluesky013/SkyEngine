//
// Created by Zach Lee on 2022/5/7.
//

#pragma once

#include <render/resources/GlobalResource.h>
#include <render/resources/RenderResource.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BasicSerialization.h>
#include <vulkan/Image.h>
#include <vector>

namespace sky {

    struct ImageAssetData {
        uint32_t width  = 0;
        uint32_t height = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        std::vector<uint8_t> data;

        template<class Archive>
        void serialize(Archive &ar)
        {
            ar(width, height, format, data);
        }
    };

    class Image : public RenderResource {
    public:
        struct Descriptor {
            VkFormat   format     = VK_FORMAT_UNDEFINED;
            VkExtent2D extent     = {1, 1};
            uint32_t   mipLevels  = 1;
            uint32_t   layers     = 1;
        };

        Image(const Descriptor& desc) : descriptor(desc)
        {
        }

        ~Image() = default;

        void InitRHI();

        bool IsValid() const override;

        VkFormat GetFormat() const;

        drv::ImagePtr GetRHIImage() const;

        void Update(const uint8_t* ptr, uint64_t size);

        static std::shared_ptr<Image> LoadFromFile(const std::string& path);

    private:
        Descriptor descriptor;
        drv::ImagePtr rhiImage;
    };
    using RDImagePtr = std::shared_ptr<Image>;

    enum class GlobalImageType {
        IMAGE_2D,
        IMAGE_CUBE_MAP
    };

    RDImagePtr CreateImage2D();

    template <>
    struct GlobalResourceTraits<Image>
    {
        using KeyType = GlobalImageType;
        static RDImagePtr Create(const GlobalImageType& value)
        {
            return CreateImage2D();
        }
    };

    namespace impl {
        void LoadFromPath(const std::string& path, ImageAssetData& data);
        void SaveToPath(const std::string& path, const ImageAssetData& data);
        RDImagePtr CreateFromData(const ImageAssetData& data);
    }

    template <>
    struct AssetTraits <Image> {
        using DataType = ImageAssetData;

        static void LoadFromPath(const std::string& path, DataType& data)
        {
            impl::LoadFromPath(path, data);
        }

        static RDImagePtr CreateFromData(const DataType& data)
        {
            return impl::CreateFromData(data);
        }

        static void SaveToPath(const std::string& path, const DataType& data)
        {
            impl::SaveToPath(path, data);
        }
    };

    using ImageAssetPtr = std::shared_ptr<Asset<Image>>;
}