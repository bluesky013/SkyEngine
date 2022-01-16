//
// Created by Zach Lee on 2022/1/13.
//

#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <map>
#include <vector>

namespace sky::drv {

    class ShaderOption {
    public:
        ShaderOption() = default;
        ~ShaderOption() = default;

        class Builder {
        public:
            Builder() = default;
            ~Builder() = default;

            void AddConstant(VkShaderStageFlagBits stage, uint32_t id, uint32_t size);

            std::shared_ptr<ShaderOption> Build();

        private:
            using ConstantMap = std::map<uint32_t, uint32_t>;
            std::map<VkShaderStageFlagBits, ConstantMap> constants;
        };

        const VkSpecializationInfo* GetSpecializationInfo(VkShaderStageFlagBits) const;


    private:
        friend class ShaderOption::Builder;
        std::unique_ptr<uint8_t[]> storage;
        std::vector<VkShaderStageFlagBits> stages;
        std::vector<VkSpecializationMapEntry> entries;
        std::vector<VkSpecializationInfo> specializationInfo;
    };

}
