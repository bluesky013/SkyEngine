//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <vector>

namespace sky::rhi {

    class Shader {
    public:
        Shader() = default;
        virtual ~Shader() = default;

        struct Descriptor {
            ShaderStageFlagBit stage;
            const uint8_t *data = nullptr;
            uint32_t size = 0;
        };

        void SetEntry(const std::string &name) { entry = name; }
        ShaderStageFlagBit GetStage() const { return stage; }
        const std::string &GetEntry() const { return entry; }

    protected:
        ShaderStageFlagBit stage = rhi::ShaderStageFlagBit::VS;
        std::string entry;
    };
    using ShaderPtr = std::shared_ptr<Shader>;
}