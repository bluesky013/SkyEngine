//
// Created by blues on 2026/10/2.
//

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <framework/asset/AssetCommon.h>

namespace sky {

    // Where a cook runs. Defaults to in-process; the loading layer is mode-agnostic (D5).
    enum class CookMode : uint32_t {
        InProcess = 0,
        OutOfProcess,
    };

    // A cook target: the product bundle plus per-kind settings (raw JSON kept opaque here).
    struct CookTarget {
        ProductBundleKey bundle = "common";
        std::string settings; // raw JSON object of per-kind settings, empty when absent
    };

    // Cook configuration (D11). Project-level presets plus asset-level overrides.
    // Effective target = asset override (from the asset's `cook` block) x project preset x platform.
    class CookConfig {
    public:
        CookConfig() = default;

        // Project-level `configs/asset_cook.jsonc` (JSON with comments).
        bool Parse(const std::string &text);

        void SetActivePlatform(std::string platform) { activePlatform = std::move(platform); }
        const std::string &GetActivePlatform() const { return activePlatform; }

        std::string ResolveTargetForPlatform(const std::string &platform) const;
        const CookTarget *FindTarget(const std::string &name) const;

        // assetCookJson: the optional `cook` block from the asset manifest entry.
        std::string ResolveTarget(const std::string &assetCookJson) const;

        // All targets to emit for an asset: the asset's override list, else every project target.
        std::vector<std::string> GetTargets(const std::string &assetCookJson) const;

        // Product bundles to build (unified with the former AssetBuilderConfig).
        const std::vector<std::string> &GetBundles() const { return bundles; }
        // Bundles for a platform preset (empty when the preset is unknown).
        std::vector<std::string> GetPresetBundles(const std::string &platform) const;

        // Cook execution mode (D5). Defaults to in-process.
        CookMode GetMode() const { return mode; }
        const std::string &GetWorkerPath() const { return workerPath; }
        uint32_t GetWorkerTimeoutMs() const { return workerTimeoutMs; }

    private:
        std::string activePlatform;
        std::unordered_map<std::string, std::string> platformTargets; // platform -> target name
        std::unordered_map<std::string, CookTarget> targets;          // target name -> definition
        std::vector<std::string> bundles;                             // all product bundles
        std::unordered_map<std::string, std::vector<std::string>> presets; // platform -> bundles

        CookMode    mode = CookMode::InProcess;
        std::string workerPath;
        uint32_t    workerTimeoutMs = 10u * 60u * 1000u;
    };

} // namespace sky
