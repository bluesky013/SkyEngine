//
// Created by Zach Lee on 2022/5/24.
//

#pragma once

#include <render/resources/Buffer.h>
#include <render/resources/RenderResource.h>
#include <render/resources/Texture.h>
#include <unordered_map>
#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetLayout.h>

namespace sky {

    struct PropertyHandler {
        uint32_t binding = 0;
        uint32_t offset  = 0;
    };

    struct ProperTableInfo {
        std::unordered_map<std::string, PropertyHandler> handleMap;
    };
    using PropertyTablePtr = std::shared_ptr<ProperTableInfo>;

    class DescriptorGroup : public RenderResource {
    public:
        DescriptorGroup()  = default;
        ~DescriptorGroup() = default;

        void UpdateTexture(uint32_t binding, const RDTexturePtr &texture);

        void UpdateBuffer(uint32_t binding, const RDBufferViewPtr &buffer);

        void Update();

        bool IsValid() const override;

        vk::DescriptorSetPtr GetRHISet() const;

        void SetPropertyTable(PropertyTablePtr table);

        PropertyTablePtr GetProperTable() const;

    private:
        friend class DescriptorPool;
        void Init();

        vk::DescriptorSetPtr                         set;
        std::unordered_map<uint32_t, RDTexturePtr>    textures;
        std::unordered_map<uint32_t, RDBufferViewPtr> buffers;
        PropertyTablePtr                              properTable;
        bool                                          dirty = true;
    };
    using RDDesGroupPtr = std::shared_ptr<DescriptorGroup>;

} // namespace sky
