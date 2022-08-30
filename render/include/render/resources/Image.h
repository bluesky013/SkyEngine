//
// Created by Zach Lee on 2022/5/7.
//

#pragma once

#include <framework/asset/Asset.h>
#include <framework/serialization/BasicSerialization.h>
#include <render/resources/GlobalResource.h>
#include <render/resources/RenderResource.h>
#include <vector>
#include <vulkan/Image.h>

namespace sky {

    struct ImageAssetData {
        uint32_t             width  = 0;
        uint32_t             height = 0;
        VkFormat             format = VK_FORMAT_UNDEFINED;
        std::vector<uint8_t> data;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(width, height, format, data);
        }
    };

    class Image : public RenderResource {
    public:
        struct Descriptor {
            VkFormat   format    = VK_FORMAT_UNDEFINED;
            VkExtent2D extent    = {1, 1};
            uint32_t   mipLevels = 1;
            uint32_t   layers    = 1;
        };

        Image(const Descriptor &desc) : descriptor(desc)
        {
        }

        ~Image() = default;

        void InitRHI();

        bool IsValid() const override;

        VkFormat GetFormat() const;

        drv::ImagePtr GetRHIImage() const;

        void Update(const uint8_t *ptr, uint64_t size);

        static std::shared_ptr<Image> CreateFromData(const ImageAssetData &data);

    private:
        Descriptor    descriptor;
        drv::ImagePtr rhiImage;
    };
    using RDImagePtr = std::shared_ptr<Image>;

    enum class GlobalImageType { IMAGE_2D, IMAGE_CUBE_MAP };

    RDImagePtr CreateImage2D();

    template <>
    struct GlobalResourceTraits<Image> {
        using KeyType = GlobalImageType;
        static RDImagePtr Create(const GlobalImageType &value)
        {
            return CreateImage2D();
        }
    };

    template <>
    struct AssetTraits<Image> {
        using DataType                                = ImageAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("E28E41C7-FC98-47B9-B86E-42CD0541A4BF");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

        static RDImagePtr CreateFromData(const DataType &data)
        {
            return Image::CreateFromData(data);
        }
    };
    using ImageAssetPtr = std::shared_ptr<Asset<Image>>;
} // namespace sky