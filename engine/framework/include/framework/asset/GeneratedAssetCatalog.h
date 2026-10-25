#pragma once

#include <core/util/Uuid.h>
#include <framework/asset/AssetProductGroup.h>

#include <map>
#include <span>

namespace sky {

    struct GeneratedAssetDescriptor {
        std::string key;
        std::string type;
        // First-import migration only; the builder must verify the old product's identity.
        Uuid verifiedLegacyId;
    };

    struct GeneratedAssetRecord {
        Uuid        uuid;
        Uuid        owner;
        std::string key;
        std::string type;
        bool        active = true;
    };

    struct GeneratedAssetManifest {
        std::vector<GeneratedAssetRecord>  records;
        std::map<std::string, std::string> defaults;
    };

    ProductStatus ParseGeneratedAssets(const Uuid &owner, const std::string &json, GeneratedAssetManifest &out);
    ProductStatus ReconcileGeneratedAssets(const Uuid                               &owner,
                                           const GeneratedAssetManifest             &previous,
                                           std::span<const GeneratedAssetDescriptor> requested,
                                           const std::map<std::string, std::string> &defaults,
                                           GeneratedAssetManifest                   &out);
    std::string   SerializeGeneratedAssets(const GeneratedAssetManifest &manifest);
} // namespace sky
