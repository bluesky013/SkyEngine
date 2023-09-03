//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Shader.h>
#include <render/RHI.h>

namespace sky {

    bool ShaderVariant::Init(rhi::ShaderStageFlagBit stage, const uint8_t *data, uint32_t size)
    {
        rhi::Shader::Descriptor desc = {};
        desc.size = size;
        desc.data = data;
        desc.stage = stage;

        shader = RHI::Get()->GetDevice()->CreateShader(desc);
        return static_cast<bool>(shader);
    }

    void Shader::AddVariant(const std::string &key, const ShaderVariantPtr &variant)
    {
        variants.emplace(key, variant);
    }

} // namespace sky
