//
// Created by Zach Lee on 2022/9/18.
//

#include <builders/technique/TechniqueBuilder.h>
#include <cereal/external/rapidjson/pointer.h>
#include <core/file/FileIO.h>
#include <render/resources/Technique.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    static const std::unordered_map<std::string, VkBlendFactor> BLEND_FACTOR_MAP =
        {
            {"ZERO", VK_BLEND_FACTOR_ZERO},
            {"ONE", VK_BLEND_FACTOR_ONE},
            {"SRC_COLOR", VK_BLEND_FACTOR_SRC_COLOR},
            {"ONE_MINUS_SRC_COLOR", VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR},
            {"DST_COLOR", VK_BLEND_FACTOR_DST_COLOR},
            {"ONE_MINUS_DST_COLOR", VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR},
            {"SRC_ALPHA", VK_BLEND_FACTOR_SRC_ALPHA},
            {"ONE_MINUS_SRC_ALPHA", VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
            {"DST_ALPHA", VK_BLEND_FACTOR_DST_ALPHA},
            {"ONE_MINUS_DST_ALPHA", VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA},
            {"CONSTANT_COLOR", VK_BLEND_FACTOR_CONSTANT_COLOR},
            {"ONE_MINUS_CONSTANT_COLOR", VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR},
            {"CONSTANT_ALPHA", VK_BLEND_FACTOR_CONSTANT_ALPHA},
            {"ONE_MINUS_CONSTANT_ALPHA", VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA},
            {"SRC_ALPHA_SATURATE", VK_BLEND_FACTOR_SRC_ALPHA_SATURATE},
            {"SRC1_COLOR", VK_BLEND_FACTOR_SRC1_COLOR},
            {"ONE_MINUS_SRC1_COLOR", VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR},
            {"SRC1_ALPHA", VK_BLEND_FACTOR_SRC1_ALPHA},
            {"ONE_MINUS_SRC1_ALPHA", VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA},
    };

    TechniqueBuilder::TechniqueBuilder()
    {

    }

    const std::vector<std::string>& TechniqueBuilder::GetExtensions() const
    {
        static const std::vector<std::string> extensions = {
            ".technique"
        };
        return extensions;
    }

    static ShaderAssetPtr GetShader(GfxTechniqueAssetData &data, rapidjson::Document &document, const std::string &shaderStage)
    {
        ShaderAssetPtr asset;
        auto &val = document["shaders"];
        if (val.HasMember(shaderStage.c_str())) {
            std::filesystem::path path = val[shaderStage.c_str()].GetString();
            auto relativePath = path.make_preferred().string();
            auto uuid = Uuid::CreateWithSeed(Fnv1a32(relativePath));
            asset = std::make_shared<Asset<Shader>>();
            asset->SetUuid(uuid);
            data.assetPathMap.emplace(uuid, relativePath);
        }
        return asset;
    }

    static void ProcessDepthStencil(const rapidjson::Document &document, vk::GraphicsPipeline::DepthStencilState &ds)
    {
        if (document.HasMember("depth_stencil")) {
            auto& dsObject = document["depth_stencil"];
            if (dsObject.HasMember("depthTestEnable")) {
                ds.depthTestEnable = dsObject["depthTestEnable"].GetBool();
            }
            if (dsObject.HasMember("depthWriteEnable")) {
                ds.depthWriteEnable = dsObject["depthWriteEnable"].GetBool();
            }
        }
    }

    static void ProcessBlendStates(const rapidjson::Document &document, vk::GraphicsPipeline::ColorBlend &blendState)
    {
        auto processBlendFactor = [](const rapidjson::Value &value, const char* factor)-> VkBlendFactor {
            VkBlendFactor ret = VK_BLEND_FACTOR_ZERO;
            if (value.HasMember(factor)) {
                std::string factorStr = value[factor].GetString();
                auto iter = BLEND_FACTOR_MAP.find(factorStr);
                if (iter != BLEND_FACTOR_MAP.end()) {
                    return iter->second;
                }
            }
            return ret;
        };

        if (document.HasMember("blend_state")) {
            auto& blendStates = document["blend_state"];
            auto array = blendStates.GetArray();
            for (auto& src : array) {
                vk::GraphicsPipeline::BlendState blend;

                if (src.HasMember("blendEnable")) {
                    blend.blendEnable = src["blendEnable"].GetBool();
                }
                blend.srcColorBlendFactor = processBlendFactor(src, "srcColor");
                blend.dstColorBlendFactor = processBlendFactor(src, "dstColor");
                blend.srcAlphaBlendFactor = processBlendFactor(src, "srcAlpha");
                blend.dstAlphaBlendFactor = processBlendFactor(src, "dstAlpha");
                blendState.blendStates.emplace_back(blend);
            }
        }
    }

    static void ProcessRasterStates(const rapidjson::Document &document, vk::GraphicsPipeline::Raster &raster)
    {
        if (document.HasMember("raster_state")) {
            auto& rasterObject = document["raster_state"];

            if (rasterObject.HasMember("depthClampEnable")) {
                raster.depthClampEnable = rasterObject["depthClampEnable"].GetBool();
            }
            if (rasterObject.HasMember("rasterizerDiscardEnable")) {
                raster.rasterizerDiscardEnable = rasterObject["rasterizerDiscardEnable"].GetBool();
            }
            if (rasterObject.HasMember("depthBiasEnable")) {
                raster.rasterizerDiscardEnable = rasterObject["depthBiasEnable"].GetBool();
            }
            if (rasterObject.HasMember("depthBiasConstantFactor")) {
                raster.depthBiasConstantFactor = rasterObject["depthBiasConstantFactor"].GetFloat();
            }
            if (rasterObject.HasMember("depthBiasClamp")) {
                raster.depthBiasClamp = rasterObject["depthBiasClamp"].GetFloat();
            }
            if (rasterObject.HasMember("depthBiasSlopeFactor")) {
                raster.depthBiasSlopeFactor = rasterObject["depthBiasSlopeFactor"].GetFloat();
            }
            if (rasterObject.HasMember("lineWidth")) {
                raster.lineWidth = rasterObject["lineWidth"].GetFloat();
            }
            if (rasterObject.HasMember("polygonMode")) {
                std::string pModeStr = rasterObject["polygonMode"].GetString();
                if (pModeStr == "FILL") raster.polygonMode = VK_POLYGON_MODE_FILL;
                else if (pModeStr == "LINE") raster.polygonMode = VK_POLYGON_MODE_LINE;
                else if (pModeStr == "POINT") raster.polygonMode = VK_POLYGON_MODE_POINT;
            }
            if (rasterObject.HasMember("frontFace")) {
                std::string pFront = rasterObject["frontFace"].GetString();
                if (pFront == "CW") raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
                else if (pFront == "CCW") raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            }
            if (rasterObject.HasMember("cullMode")) {
                std::string pCull = rasterObject["cullMode"].GetString();
                if (pCull == "NONE") raster.cullMode = VK_CULL_MODE_NONE;
                if (pCull == "FRONT") raster.cullMode = VK_CULL_MODE_FRONT_BIT;
                if (pCull == "BACK") raster.cullMode = VK_CULL_MODE_BACK_BIT;
                if (pCull == "FRONT_AND_BACK") raster.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
            }
        }
    }

    static void ProcessTag(rapidjson::Document &document, GfxTechniqueAssetData &data)
    {
        if (document.HasMember("view_tag")) {
            data.viewTag = document["view_tag"].GetUint();
        }
        if (document.HasMember("draw_tag")) {
            data.drawTag = document["draw_tag"].GetUint();
        }
    }

    static void ProcessGraphics(const std::string& dataPath, const std::filesystem::path& path, rapidjson::Document &document)
    {
        auto fileName = path.filename().replace_extension();
        auto asset = std::make_shared<Asset<GraphicsTechnique>>();
        GfxTechniqueAssetData data;

        data.vs = GetShader(data, document, "vert");
        data.fs = GetShader(data, document, "frag");

        ProcessDepthStencil(document, data.depthStencilState);
        ProcessBlendStates(document, data.blends);
        ProcessRasterStates(document, data.raster);
        ProcessTag(document, data);
        asset->SetData(std::move(data));

        std::filesystem::path outPath = dataPath;
        std::stringstream ss;
        ss << fileName.string() << ".tech";
        auto outputPath = outPath.append(ss.str());
        AssetManager::Get()->SaveAsset(asset, outputPath.string());
    }

    void TechniqueBuilder::Build(const std::string& projectPath, const std::filesystem::path& path) const
    {
        auto fileName = path.filename();

        std::string json;
        ReadString(path.string(), json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("type")) {
            return;
        }

        std::filesystem::path dataPath(projectPath);
        dataPath.append("data").append("techniques");
        if (!std::filesystem::exists(dataPath)) {
            std::filesystem::create_directories(dataPath);
        }

        std::string type = document["type"].GetString();
        if (type == "graphics") {
            ProcessGraphics(dataPath.string(), path, document);
        }
    }
}
