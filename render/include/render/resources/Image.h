//
// Created by Zach Lee on 2022/5/7.
//

#pragma once

#include <render/resources/GlobalResource.h>
#include <render/resources/RenderResource.h>
#include <vulkan/Image.h>

namespace sky {

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

        void InitRHI() override;

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
}