//
// Created by Zach Lee on 2022/1/16.
//

#pragma once

#include <vulkan/Shader.h>
#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>

namespace sky {

    class ShaderAsset : public AssetBase {
    public:
        ShaderAsset(const Uuid& id) : AssetBase(id) {}
        ~ShaderAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("1338d2ed-5d6d-4324-aba5-1bfb6908fd7a");

        struct ShaderData {
            std::vector<uint32_t> data;
            VkShaderStageFlagBits stage;
            std::string entry = "main";
        };

        struct SourceData {
            std::vector<ShaderData> shaders;
        };

    private:
        const Uuid& GetType() const override { return TYPE; }
        std::vector<SourceData> shaderDataList;
    };

    class Shader : public ResourceBase {
    public:
        Shader(const Uuid& id) : ResourceBase(id) {}
        ~Shader() = default;

    private:
    };

}