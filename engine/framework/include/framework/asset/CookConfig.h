//
// Created by blues on 2026/10/2.
//

#pragma once

#include <framework/asset/AssetCommon.h>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky {

    // Where a cook runs. Defaults to in-process; the loading layer is mode-agnostic (D5).
    enum class CookMode : uint32_t {
        InProcess = 0,
        OutOfProcess,
    };

    // A cook target: the product bundle it resolves to. Per-kind settings live in the builder's preset
    // file (e.g. image_build_presets.json) keyed by bundle, not here.
    struct CookTarget {
        ProductBundleKey bundle = "common";
    };

    // Cook configuration (D11). Project-level presets plus asset-level overrides.
    // Effective target = asset override (from the asset's `cook` block) x project preset x platform.
    class CookConfig {
    public:
        CookConfig() = default;

        // Project-level `configs/asset_cook.jsonc` (JSON with comments).
        bool Parse(const std::string &text);

        void SetActivePlatform(std::string platform)
        {
            activePlatform = std::move(platform);
        }
        const std::string &GetActivePlatform() const
        {
            return activePlatform;
        }

        std::string       ResolveTargetForPlatform(const std::string &platform) const;
        const CookTarget *FindTarget(const std::string &name) const;

        // assetCookJson: the optional `cook` block from the asset manifest entry.
        std::string ResolveTarget(const std::string &assetCookJson) const;

        // All targets to emit for an asset: the asset's override list, else the project's declared
        // targets, else the platform's preset bundles. Single source of truth for target resolution.
        std::vector<std::string> GetTargets(const std::string &assetCookJson) const;
        std::vector<std::string> GetTargets(const std::string &assetCookJson, const std::string &platform) const;

        // Product bundle a target resolves to (`targets[name].bundle`), or the target name itself when the
        // target is not declared. Presets are keyed by bundle; overrides/state are keyed by target.
        std::string ResolveBundleForTarget(const std::string &target) const;

        // Per-setting override for a target/bundle from the asset's `cook.settings[target]` (flat
        // scalars only; non-scalar values and non-string keys are ignored). Empty when absent.
        std::map<std::string, std::string> GetTargetSettings(const std::string &assetCookJson, const std::string &target) const;

        // Product bundles to build (unified with the former AssetBuilderConfig).
        const std::vector<std::string> &GetBundles() const
        {
            return bundles;
        }
        // Bundles for a platform preset (empty when the preset is unknown).
        std::vector<std::string> GetPresetBundles(const std::string &platform) const;

        // Cook execution mode (D5). Defaults to in-process.
        CookMode GetMode() const
        {
            return mode;
        }
        const std::string &GetWorkerPath() const
        {
            return workerPath;
        }
        uint32_t GetWorkerTimeoutMs() const
        {
            return workerTimeoutMs;
        }

    private:
        std::string activePlatform;
        // Platform model (two tables, distinct roles):
        //   - `platforms` (platformTargets): the platform's default/primary target, used for the single
        //     on-demand cook target (AssetDataBase::GetTarget). Optional.
        //   - `presets` (presets): the platform's product **bundles** to build (batch fallback when the
        //     asset/project declares no targets). Multiple platforms may list the same bundle.
        // Targets: `targets` (targets) is the declared cook-target universe (name -> bundle); per-asset
        // `cook.targets` overrides; otherwise `presets` is used. Override/state key = target, preset key =
        // bundle (see ResolveBundleForTarget).
        std::unordered_map<std::string, std::string>              platformTargets; // platform -> target name
        std::unordered_map<std::string, CookTarget>               targets;         // target name -> definition
        std::vector<std::string>                                  bundles;         // all product bundles
        std::unordered_map<std::string, std::vector<std::string>> presets;         // platform -> bundles

        CookMode    mode = CookMode::InProcess;
        std::string workerPath;
        uint32_t    workerTimeoutMs = 10u * 60u * 1000u;
    };

} // namespace sky
