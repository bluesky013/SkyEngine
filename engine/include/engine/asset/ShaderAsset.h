//
// Created by Zach Lee on 2022/1/16.
//

#pragma once

#include <vulkan/Shader.h>
#include <vulkan/GraphicsPipeline.h>
#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>
#include <engine/BasicSerialization.h>

namespace sky {

    struct ShaderData {
        std::vector<uint32_t> data;
        VkShaderStageFlagBits stage;
        std::string entry = "main";

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(data, stage, entry);
        }
    };

    struct ShaderSourceData {
        std::vector<ShaderData> shaders;

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(shaders);
        }
    };

    class ShaderAsset : public AssetBase {
    public:
        ShaderAsset(const Uuid& id) : AssetBase(id) {}
        ~ShaderAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("1338d2ed-5d6d-4324-aba5-1bfb6908fd7a");

        const ShaderSourceData& GetSourceData() const
        {
            return sourceData;
        }

        template<class Archive>
        void load(Archive& ar)
        {
            ar(sourceData);
        }

        template<class Archive>
        void save(Archive& ar) const
        {
            ar(sourceData);
        }

        ShaderSourceData sourceData;

        const Uuid& GetType() const override { return TYPE; }
    };

    using ShaderAssetPtr = CounterPtr<ShaderAsset>;

    class Shader : public ResourceBase {
    public:
        Shader(const Uuid& id) : ResourceBase(id) {}
        ~Shader() = default;

        static CounterPtr<Shader> CreateFromAsset(AssetPtr asset);

        const drv::GraphicsPipeline::Program& GetProgram() const;

        const drv::GraphicsPipeline::State& GetState() const;

    private:
        drv::GraphicsPipeline::Program program;
        drv::GraphicsPipeline::State state;
        drv::PipelineLayoutPtr pipelineLayout;
    };
    using ShaderPtr = CounterPtr<Shader>;

}