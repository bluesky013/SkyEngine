
//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/TechniqueBuilder.h>

#include <filesystem>

#include <core/file/FileIO.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>


namespace sky::builder {

    std::unordered_map<std::string_view, rhi::BlendFactor> BLEND_FACTOR_MAP = {
        {"ZERO",                     rhi::BlendFactor::ZERO                    },
        {"ONE",                      rhi::BlendFactor::ONE                     },
        {"SRC_COLOR",                rhi::BlendFactor::SRC_COLOR               },
        {"ONE_MINUS_SRC_COLOR",      rhi::BlendFactor::ONE_MINUS_SRC_COLOR     },
        {"DST_COLOR",                rhi::BlendFactor::DST_COLOR               },
        {"ONE_MINUS_DST_COLOR",      rhi::BlendFactor::ONE_MINUS_DST_COLOR     },
        {"SRC_ALPHA",                rhi::BlendFactor::SRC_ALPHA               },
        {"ONE_MINUS_SRC_ALPHA",      rhi::BlendFactor::ONE_MINUS_SRC_ALPHA     },
        {"DST_ALPHA",                rhi::BlendFactor::DST_ALPHA               },
        {"ONE_MINUS_DST_ALPHA",      rhi::BlendFactor::ONE_MINUS_DST_ALPHA     },
        {"CONSTANT_COLOR",           rhi::BlendFactor::CONSTANT_COLOR          },
        {"ONE_MINUS_CONSTANT_COLOR", rhi::BlendFactor::ONE_MINUS_CONSTANT_COLOR},
        {"CONSTANT_ALPHA",           rhi::BlendFactor::CONSTANT_ALPHA          },
        {"ONE_MINUS_CONSTANT_ALPHA", rhi::BlendFactor::ONE_MINUS_CONSTANT_ALPHA},
        {"SRC_ALPHA_SATURATE",       rhi::BlendFactor::SRC_ALPHA_SATURATE      },
        {"SRC1_COLOR",               rhi::BlendFactor::SRC1_COLOR              },
        {"ONE_MINUS_SRC1_COLOR",     rhi::BlendFactor::ONE_MINUS_SRC1_COLOR    },
        {"SRC1_ALPHA",               rhi::BlendFactor::SRC1_ALPHA              },
        {"ONE_MINUS_SRC1_ALPHA",     rhi::BlendFactor::ONE_MINUS_SRC1_ALPHA    },
    };

    std::unordered_map<std::string_view, RenderVertexFlagBit> VTX_FLAG_MAP = {
        {"SKIN",     RenderVertexFlagBit::SKIN},
        {"INSTANCE", RenderVertexFlagBit::INSTANCE},
    };

    static void ProcessShader(rapidjson::Document &document, ShaderRefData &shaderRef, TechAssetType type)
    {
        auto &val = document["shader"];

        if (type == TechAssetType::MESH) {
            if (val.HasMember("object")) {
                shaderRef.objectOrCSMain = val["object"].GetString();
            }

            if (val.HasMember("mesh")) {
                shaderRef.vertOrMeshMain = val["mesh"].GetString();
            }
        } else {
            if (val.HasMember("vertex")) {
                shaderRef.vertOrMeshMain = val["vertex"].GetString();
            }
        }

        if (val.HasMember("fragment")) {
            shaderRef.fragmentMain = val["fragment"].GetString();
        }

        if (val.HasMember("path")) {
            const auto *path = val["path"].GetString();
            auto src = AssetDataBase::Get()->RegisterAsset(path);
            shaderRef.shader = src->uuid;
        }
    }

    static void GetPassInfo(rapidjson::Document &document, TechniqueAssetData &data)
    {
        auto &val = document["pass"];
        if (val.HasMember("tag")) {
            data.passTag = val["tag"].GetString();
        }
    }

    static void GetVariants(rapidjson::Document &document, TechniqueAssetData &data)
    {
        if (document.HasMember("pre_defines")) {
            auto array = document["pre_defines"].GetArray();
            for (auto &def : array) {
                data.preDefines.emplace_back(def.GetString());
            }
        }

        if (document.HasMember("vertex_options")) {
            auto object = document["vertex_options"].GetObject();
            for (auto iter = object.MemberBegin(); iter != object.MemberEnd(); ++iter) {
                const auto &member = *iter;
                auto flag  = VTX_FLAG_MAP.find(member.name.GetString());
                if (flag != VTX_FLAG_MAP.end()) {
                    data.vertexFlags.emplace_back(TechniqueVertexFlags {
                        flag->second,
                        member.value.GetString()
                    });
                }
            }
        }
    }

    static void ProcessDepthStencil(const rapidjson::Document &document, rhi::DepthStencil &ds)
    {
        if (document.HasMember("depth_stencil")) {
            const auto& dsObject = document["depth_stencil"];
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
            const auto& states = document["blend_state"];
            auto array = states.GetArray();
            for (const auto& src : array) {
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
            const auto& rasterObject = document["raster_state"];

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
                if (pModeStr == "FILL") { raster.polygonMode = rhi::PolygonMode::FILL;
                } else if (pModeStr == "LINE") { raster.polygonMode = rhi::PolygonMode::LINE;
                } else if (pModeStr == "POINT") { raster.polygonMode = rhi::PolygonMode::POINT;}
            }
            if (rasterObject.HasMember("frontFace")) {
                std::string pFront = rasterObject["frontFace"].GetString();
                if (pFront == "CW") { raster.frontFace = rhi::FrontFace::CW;
                } else if (pFront == "CCW") { raster.frontFace = rhi::FrontFace::CCW; }
            }
            if (rasterObject.HasMember("cullMode")) {
                std::string pCull = rasterObject["cullMode"].GetString();
                if (pCull == "NONE") { raster.cullMode = rhi::CullModeFlagBits::NONE;
                } else if (pCull == "FRONT") { raster.cullMode = rhi::CullModeFlagBits::FRONT;
                } else if (pCull == "BACK") { raster.cullMode = rhi::CullModeFlagBits::BACK;
                } else if (pCull == "FRONT_AND_BACK") { raster.cullMode = rhi::CullModeFlagBits::FRONT | rhi::CullModeFlagBits::BACK;}
            }
        }
    }

    static void ProcessGraphics(rapidjson::Document &document, TechniqueAssetData &data, const AssetBuildRequest &request)
    {
        ProcessShader(document, data.shader, data.type);
        ProcessDepthStencil(document, data.depthStencil);
        ProcessBlendStates(document, data.blendStates);
        ProcessRasterStates(document, data.rasterState);
        GetPassInfo(document, data);
        GetVariants(document, data);
    }

    void TechniqueBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        std::string json;
        request.file->ReadString(json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("type")) {
            return;
        }

        auto *am = AssetManager::Get();
        std::string type = document["type"].GetString();
        if (type == "graphics") {
            auto asset = am->FindOrCreateAsset<Technique>(request.assetInfo->uuid);
            TechniqueAssetData &assetData = asset->Data();
            ProcessGraphics(document, assetData, request);

            asset->AddDependencies(assetData.shader.shader);
            AssetManager::Get()->SaveAsset(asset, request.target);
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

}
