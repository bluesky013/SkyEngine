
//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/TechniqueBuilder.h>
#include <render/assets/Technique.h>
#include <core/file/FileIO.h>
#include <framework/asset/AssetManager.h>
#include <rapidjson/rapidjson.h>
#include <filesystem>

namespace sky::builder {

    std::unordered_map<std::string, rhi::BlendFactor> BLEND_FACTOR_MAP = {
        {"ZERO                    ", rhi::BlendFactor::ZERO                    },
        {"ONE                     ", rhi::BlendFactor::ONE                     },
        {"SRC_COLOR               ", rhi::BlendFactor::SRC_COLOR               },
        {"ONE_MINUS_SRC_COLOR     ", rhi::BlendFactor::ONE_MINUS_SRC_COLOR     },
        {"DST_COLOR               ", rhi::BlendFactor::DST_COLOR               },
        {"ONE_MINUS_DST_COLOR     ", rhi::BlendFactor::ONE_MINUS_DST_COLOR     },
        {"SRC_ALPHA               ", rhi::BlendFactor::SRC_ALPHA               },
        {"ONE_MINUS_SRC_ALPHA     ", rhi::BlendFactor::ONE_MINUS_SRC_ALPHA     },
        {"DST_ALPHA               ", rhi::BlendFactor::DST_ALPHA               },
        {"ONE_MINUS_DST_ALPHA     ", rhi::BlendFactor::ONE_MINUS_DST_ALPHA     },
        {"CONSTANT_COLOR          ", rhi::BlendFactor::CONSTANT_COLOR          },
        {"ONE_MINUS_CONSTANT_COLOR", rhi::BlendFactor::ONE_MINUS_CONSTANT_COLOR},
        {"CONSTANT_ALPHA          ", rhi::BlendFactor::CONSTANT_ALPHA          },
        {"ONE_MINUS_CONSTANT_ALPHA", rhi::BlendFactor::ONE_MINUS_CONSTANT_ALPHA},
        {"SRC_ALPHA_SATURATE      ", rhi::BlendFactor::SRC_ALPHA_SATURATE      },
        {"SRC1_COLOR              ", rhi::BlendFactor::SRC1_COLOR              },
        {"ONE_MINUS_SRC1_COLOR    ", rhi::BlendFactor::ONE_MINUS_SRC1_COLOR    },
        {"SRC1_ALPHA              ", rhi::BlendFactor::SRC1_ALPHA              },
        {"ONE_MINUS_SRC1_ALPHA    ", rhi::BlendFactor::ONE_MINUS_SRC1_ALPHA    },
    };

    static std::string GetShader(TechniqueAssetData &data, rapidjson::Document &document, const std::string &shaderStage)
    {
        auto *am = AssetManager::Get();

        auto &val = document["shaders"];
        if (val.HasMember(shaderStage.c_str())) {
            auto relativePath = val[shaderStage.c_str()].GetString();
            std::string fullPath = am->GetRealPath(relativePath);
//            if (!am->IsImported(fullPath)) {
                AssetManager::Get()->ImportSource(fullPath);
//            }
            return fullPath;
        }
        return "";
    }

    static void ProcessDepthStencil(const rapidjson::Document &document, rhi::DepthStencil &ds)
    {
        if (document.HasMember("depth_stencil")) {
            auto& dsObject = document["depth_stencil"];
            if (dsObject.HasMember("depthTestEnable")) {
                ds.depthTest = dsObject["depthTestEnable"].GetBool();
            }
            if (dsObject.HasMember("depthWriteEnable")) {
                ds.depthWrite = dsObject["depthWriteEnable"].GetBool();
            }
        }
    }

    static void ProcessBlendStates(const rapidjson::Document &document, std::vector<rhi::BlendState> &blendStates)
    {
        auto processBlendFactor = [](const rapidjson::Value &value, const char* factor, rhi::BlendFactor dft)-> rhi::BlendFactor {
            if (value.HasMember(factor)) {
                std::string factorStr = value[factor].GetString();
                auto iter = BLEND_FACTOR_MAP.find(factorStr);
                if (iter != BLEND_FACTOR_MAP.end()) {
                    return iter->second;
                }
            }
            return dft;
        };

        if (document.HasMember("blend_state")) {
            auto& states = document["blend_state"];
            auto array = states.GetArray();
            for (auto& src : array) {
                rhi::BlendState blend;

                if (src.HasMember("blendEnable")) {
                    blend.blendEn = src["blendEnable"].GetBool();
                }
                blend.srcColor = processBlendFactor(src, "srcColor", rhi::BlendFactor::ONE);
                blend.dstColor = processBlendFactor(src, "dstColor", rhi::BlendFactor::ZERO);
                blend.srcAlpha = processBlendFactor(src, "srcAlpha", rhi::BlendFactor::ONE);
                blend.dstAlpha = processBlendFactor(src, "dstAlpha", rhi::BlendFactor::ZERO);
                blendStates.emplace_back(blend);
            }
        }
    }

    static void ProcessRasterStates(const rapidjson::Document &document, rhi::RasterState &raster)
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
                if (pModeStr == "FILL") raster.polygonMode = rhi::PolygonMode::FILL;
                else if (pModeStr == "LINE") raster.polygonMode = rhi::PolygonMode::LINE;
                else if (pModeStr == "POINT") raster.polygonMode = rhi::PolygonMode::POINT;
            }
            if (rasterObject.HasMember("frontFace")) {
                std::string pFront = rasterObject["frontFace"].GetString();
                if (pFront == "CW") raster.frontFace = rhi::FrontFace::CW;
                else if (pFront == "CCW") raster.frontFace = rhi::FrontFace::CCW;
            }
            if (rasterObject.HasMember("cullMode")) {
                std::string pCull = rasterObject["cullMode"].GetString();
                if (pCull == "NONE") raster.cullMode = rhi::CullModeFlagBits::NONE;
                if (pCull == "FRONT") raster.cullMode = rhi::CullModeFlagBits::FRONT;
                if (pCull == "BACK") raster.cullMode = rhi::CullModeFlagBits::BACK;
                if (pCull == "FRONT_AND_BACK") raster.cullMode = rhi::CullModeFlagBits::FRONT | rhi::CullModeFlagBits::BACK;
            }
        }
    }

    static void ProcessGraphics(const std::filesystem::path& path, rapidjson::Document &document, TechniqueAssetData &data)
    {
        data.shaders.emplace_back(GetShader(data, document, "vert"));
        data.shaders.emplace_back(GetShader(data, document, "frag"));

        ProcessDepthStencil(document, data.depthStencil);
        ProcessBlendStates(document, data.blendStates);
        ProcessRasterStates(document, data.rasterState);
    }

    void TechniqueBuilder::Request(BuildRequest &request)
    {
        std::string json;
        ReadString(request.fullPath, json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("type")) {
            return;
        }

        AssetManager *am = AssetManager::Get();
        std::filesystem::path fullPath(request.fullPath);
        std::filesystem::path outPath(request.projectDir);
        outPath.append("techniques");
        std::filesystem::create_directories(outPath);

        outPath.append(request.name);
        std::string type = document["type"].GetString();
        if (type == "graphics") {

            auto asset = am->CreateAsset<Technique>(outPath.make_preferred().string());
            TechniqueAssetData &assetData = asset->Data();
            ProcessGraphics(request.fullPath, document, assetData);
            request.products.emplace_back(BuildProduct{asset->GetUuid()});
            am->SaveAsset(asset);
        }
    }

}