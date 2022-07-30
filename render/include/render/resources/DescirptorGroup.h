//
// Created by Zach Lee on 2022/5/24.
//


#pragma once

#include <render/resources/RenderResource.h>
#include <render/resources/Texture.h>
#include <render/resources/Buffer.h>
#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetLayout.h>

namespace sky {

    class DescriptorGroup : public RenderResource {
    public:
        DescriptorGroup() = default;
        ~DescriptorGroup() = default;

        void UpdateTexture(uint32_t binding, const RDTexturePtr& texture);

        void UpdateBuffer(uint32_t binding, const RDBufferViewPtr& buffer);

        void Update();

        bool IsValid() const override;

        drv::DescriptorSetPtr GetRHISet() const;

    private:
        friend class DescriptorPool;
        void Init();

        drv::DescriptorSetPtr set;
        std::unordered_map<uint32_t, RDTexturePtr> textures;
        std::unordered_map<uint32_t, RDBufferViewPtr> buffers;
        bool dirty = true;
    };
    using RDDesGroupPtr = std::shared_ptr<DescriptorGroup>;

}