//
// Created by Zach Lee on 2023/9/9.
//

#include <builder/render/VertexLibraryBuilder.h>

#include <filesystem>

#include <rapidjson/document.h>

#include <core/file/FileIO.h>
#include <framework/asset/AssetManager.h>

#include <rhi/Core.h>
#include <render/VertexDescLibrary.h>
#include <render/adaptor/assets/VertexDescLibraryAsset.h>

namespace sky::builder {

    std::unordered_map<std::string, rhi::Format> FORMAT_MAP = {
        {"F_R32",    rhi::Format::F_R32   },
        {"F_RG32",   rhi::Format::F_RG32  },
        {"F_RGB32",  rhi::Format::F_RGB32 },
        {"F_RGBA32", rhi::Format::F_RGBA32},
        {"F_R8",     rhi::Format::F_R8    },
        {"F_RG8",    rhi::Format::F_RG8   },
        {"F_RGBA8",  rhi::Format::F_RGBA8 },
    };

    std::unordered_map<std::string, rhi::VertexInputRate> RATE_MAP = {
        {"PER_VERTEX",     rhi::VertexInputRate::PER_VERTEX   },
        {"PER_INSTANCE",   rhi::VertexInputRate::PER_INSTANCE  },
    };

    template <typename T>
    static void ProcessDescriptions(rhi::VertexInput::Descriptor &desc, const T &value)
    {
        if (value.HasMember("attributes")) {
            auto &attributes = value["attributes"];
            auto array = attributes.GetArray();
            desc.attributes.reserve(array.Size());
            for (auto &attribute : array) {
                desc.attributes.emplace_back();
                rhi::VertexAttributeDesc &vtxAttribute = desc.attributes.back();
                if (attribute.HasMember("location")) {
                    vtxAttribute.location = attribute["location"].GetUint();
                }
                if (attribute.HasMember("binding")) {
                    vtxAttribute.binding = attribute["binding"].GetUint();
                }
                if (attribute.HasMember("offset")) {
                    vtxAttribute.offset = attribute["offset"].GetUint();
                }
                if (attribute.HasMember("format")) {
                    auto fmtStr = attribute["format"].GetString();
                    auto iter = FORMAT_MAP.find(fmtStr);
                    vtxAttribute.format = iter != FORMAT_MAP.end() ? iter->second : rhi::Format::UNDEFINED;
                }
            }
        }
        if (value.HasMember("bindings")) {
            auto &bindings = value["bindings"];
            auto array     = bindings.GetArray();
            desc.bindings.reserve(array.Size());
            for (auto &binding : array) {
                desc.bindings.emplace_back();
                rhi::VertexBindingDesc &vtxBinding = desc.bindings.back();
                if (binding.HasMember("binding")) {
                    vtxBinding.binding = binding["binding"].GetUint();
                }
                if (binding.HasMember("stride")) {
                    vtxBinding.stride = binding["stride"].GetUint();
                }
                if (binding.HasMember("rate")) {
                    auto fmtStr = binding["rate"].GetString();
                    auto iter = RATE_MAP.find(fmtStr);
                    vtxBinding.inputRate = iter != RATE_MAP.end() ? iter->second : rhi::VertexInputRate::PER_VERTEX;
                }
            }
        }
    }

    void VertexLibraryBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        if (!request.buildKey.empty() && request.buildKey != std::string(KEY)) {
            return;
        }

        std::string json;
        ReadString(request.fullPath, json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("descriptions")) {
            return;
        }

        auto &val = document["descriptions"];
        if (!val.IsArray()) {
            return;
        }

        auto *am = AssetManager::Get();
        std::filesystem::path fullPath(request.fullPath);

        auto asset = am->CreateAsset<VertexDescLibrary>(request.uuid);
        auto &assetData = asset->Data();

        auto array = val.GetArray();
        for (auto &desc : array) {
            for (auto iter = desc.MemberBegin(); iter != desc.MemberEnd(); ++iter) {
                ProcessDescriptions(assetData.descriptions[iter->name.GetString()], iter->value);
            }
        }
        result.products.emplace_back(BuildProduct{KEY.data(), asset});
        result.success = true;
    }

} // namespace sky::builder
