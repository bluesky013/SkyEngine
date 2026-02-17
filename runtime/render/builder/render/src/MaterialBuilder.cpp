//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/MaterialBuilder.h>

#include <filesystem>
#include <algorithm>
#include <rapidjson/document.h>

#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/logger/Logger.h>

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetBuilderManager.h>

static const char* TAG = "MaterialBuilder";

namespace sky::builder {

    using JsonValue = rapidjson::Document::GenericValue;
    static uint8_t ProcessFilter(std::string mode)
    {
        std::transform(mode.begin(), mode.end(), mode.begin(),
            [](unsigned char c){ return std::tolower(c); });

        if (mode == "nearest") {
            return static_cast<uint8_t>(rhi::Filter::NEAREST);
        }
        if (mode == "linear") {
            return static_cast<uint8_t>(rhi::Filter::LINEAR);
        }

        SKY_ASSERT(0)
        return static_cast<uint8_t>(rhi::Filter::LINEAR);
    }

    static uint8_t ProcessMipFilter(std::string mode)
    {
        std::transform(mode.begin(), mode.end(), mode.begin(),
            [](unsigned char c){ return std::tolower(c); });

        if (mode == "nearest") {
            return static_cast<uint8_t>(rhi::MipFilter::NEAREST);
        }
        if (mode == "linear") {
            return static_cast<uint8_t>(rhi::MipFilter::LINEAR);
        }

        SKY_ASSERT(0)
        return static_cast<uint8_t>(rhi::MipFilter::LINEAR);
    }

    static uint8_t ProcessWrapMode(std::string mode)
    {
        std::transform(mode.begin(), mode.end(), mode.begin(),
            [](unsigned char c){ return std::tolower(c); });

        if (mode == "repeat") {
            return static_cast<uint8_t>(rhi::WrapMode::REPEAT);
        }
        if (mode == "clamp_to_edge") {
            return static_cast<uint8_t>(rhi::WrapMode::CLAMP_TO_EDGE);
        }

        SKY_ASSERT(0)
        return static_cast<uint8_t>(rhi::WrapMode::REPEAT);
    }

    TextureSampler ProcessSampler(const JsonValue &val)
    {
        TextureSampler sampler = {
            .magFilter = static_cast<uint8_t>(rhi::Filter::LINEAR),
            .minFilter = static_cast<uint8_t>(rhi::Filter::LINEAR),
            .mipMode = static_cast<uint8_t>(rhi::MipFilter::LINEAR),
            .addressModeU = static_cast<uint8_t>(rhi::WrapMode::REPEAT),
            .addressModeV = static_cast<uint8_t>(rhi::WrapMode::REPEAT),
            .addressModeW = static_cast<uint8_t>(rhi::WrapMode::REPEAT),
        };

        if (val.HasMember("MagFilter")) {
            sampler.magFilter = ProcessFilter(val["MagFilter"].GetString());
        }
        if (val.HasMember("MinFilter")) {
            sampler.minFilter = ProcessFilter(val["MinFilter"].GetString());
        }
        if (val.HasMember("MipMode")) {
            sampler.mipMode = ProcessMipFilter(val["MipMode"].GetString());
        }
        if (val.HasMember("AddressModeU")) {
            sampler.addressModeU = ProcessWrapMode(val["AddressModeU"].GetString());
        }
        if (val.HasMember("AddressModeV")) {
            sampler.addressModeV = ProcessWrapMode(val["AddressModeU"].GetString());
        }
        if (val.HasMember("AddressModeW")) {
            sampler.addressModeW = ProcessWrapMode(val["AddressModeU"].GetString());
        }
        return sampler;
    }

    void ProcessProperties(rapidjson::Document &document, MaterialProperties &properties, const AssetBuildRequest &request, AssetBase& asset)
    {
        if (!document.HasMember("properties")) {
            return;
        }

        auto &proValues = document["properties"];
        if (proValues.IsObject()) {
            for (auto iter = proValues.MemberBegin(); iter != proValues.MemberEnd(); ++iter) {
                auto &obj = iter->value;
                auto name = iter->name.GetString();

                if (obj.IsString()) {
                    const auto *imagePath = obj.GetString();
                    auto        tex = AssetDataBase::Get()->RegisterAsset(imagePath);
                    properties.valueMap[name] = MaterialTexture{tex->uuid};
                    request.assetInfo->dependencies.emplace_back(tex->uuid);
                    asset.AddDependencies(tex->uuid);
                    AssetBuilderManager::Get()->BuildRequest(tex->uuid, request.target);
                } else if (obj.IsObject()) {
                    properties.valueMap[name] = ProcessSampler(obj);
                } else if (obj.IsFloat()) {
                    properties.valueMap[name] = obj.GetFloat();
                } else if (obj.IsUint()) {
                    properties.valueMap[name] = static_cast<uint32_t>(obj.GetUint());
                } else if (obj.IsInt()) {
                    properties.valueMap[name] = static_cast<int32_t>(obj.GetInt());
                } else if (obj.IsBool()) {
                    properties.valueMap[name] = static_cast<uint32_t>(obj.GetBool());
                } else if (obj.IsArray()) {
                    auto   array = obj.GetArray();
                    float *v     = nullptr;
                    if (array.Size() == 2) {
                        v = std::get<Vector2>(properties.valueMap.emplace(name, Vector2{}).first->second).v;
                    } else if (array.Size() == 3) {
                        v = std::get<Vector3>(properties.valueMap.emplace(name, Vector3{}).first->second).v;
                    } else if (array.Size() == 4) {
                        v = std::get<Vector4>(properties.valueMap.emplace(name, Vector4{}).first->second).v;
                    }
                    for (auto &val : array) {
                        (*v) = val.GetFloat();
                        ++v;
                    }
                }
            }
        }
    }

    void MaterialBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        if (request.assetInfo->ext == ".mat") {
            BuildMaterial(request, result);
        } else if (request.assetInfo->ext == ".mati") {
            BuildMaterialInstance(request, result);
        }
    }

    void MaterialBuilder::BuildMaterialInstance(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        std::string json;
        request.file->ReadString(json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("material")) {
            return;
        }
        auto *am = AssetManager::Get();
        auto asset = am->FindOrCreateAsset<MaterialInstance>(request.assetInfo->uuid);
        auto &assetData = asset->Data();

        const auto *materialPath = document["material"].GetString();
        auto mat = AssetDataBase::Get()->RegisterAsset(materialPath);
        if (!mat) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        assetData.material = mat->uuid;
        asset->AddDependencies(mat->uuid);

        ProcessProperties(document, assetData.properties, request, *asset);
        AssetManager::Get()->SaveAsset(asset, request.target);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

    void MaterialBuilder::BuildMaterial(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        std::string json;
        request.file->ReadString(json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("techniques")) {
            return;
        }

        auto *am = AssetManager::Get();
        auto asset = am->FindOrCreateAsset<Material>(request.assetInfo->uuid);
        auto &assetData = asset->Data();

        auto &val = document["techniques"];
        if (val.IsArray()) {
            auto techArray = val.GetArray();
            for (auto &tech : techArray) {
                if (tech.IsString()) {
                    const auto *techPath = tech.GetString();
                    auto techId = AssetDataBase::Get()->RegisterAsset(techPath);
                    if (!techId) {
                        LOG_W(TAG, "Material %s import technique failed %s", request.assetInfo->uuid.ToString().c_str(), techPath);
                        continue;
                    }
                    asset->AddDependencies(techId->uuid);

                    std::string u = techId->uuid.ToString();

                    AssetBuilderManager::Get()->BuildRequest(techId->uuid, request.target);
                    request.assetInfo->dependencies.emplace_back(techId->uuid);
                    assetData.techniques.emplace(techId->uuid);
                }
            }
        }

        ProcessProperties(document, assetData.defaultProperties, request, *asset);
        AssetManager::Get()->SaveAsset(asset, request.target);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }
}
