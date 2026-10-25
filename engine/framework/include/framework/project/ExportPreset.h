//
// Created on 2026/10/12.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sky {

    // One CMake cache entry of an export preset (`cacheVariables`). Modelled as an
    // ordered key/value pair so the reflected form (which has no map widget) can
    // edit it as a sequence; on disk it stays a JSON object.
    struct ExportPresetCacheVariable {
        std::string key;
        std::string value;
    };

    // Edited as typed entries; serialized as the target-keyed object consumed by the shader cooker.
    struct ExportPresetShaderEnvironment {
        std::string              target;
        uint32_t                 codeFormat = 0; // Zero omits the override and selects the target default.
        std::string              profile;
        uint32_t                 minimumVersion       = 0;
        uint64_t                 requiredCapabilities = 0;
        uint32_t                 matrixLayout         = 0;
        std::vector<std::string> options;
        bool                     operator==(const ExportPresetShaderEnvironment &) const = default;
    };

    // One packaging distribution (an entry of `export_presets.json`). The same
    // schema is consumed by python/package_project.py and edited in the editor
    // through the generic reflection panel. The field set is defined by
    // configs/export_preset.schema.json (single source of truth).
    struct ExportPreset {
        std::string name;
        std::string platform; // android | ios | macos | windows | linux
        std::string variant = "debug";

        std::string bundleId; // install id; defaults to the project application id
        std::string versionName = "1.0";
        int         versionCode = 1;

        // Apple signing / bundle.
        std::string signing = "auto"; // auto | off
        bool        bundle  = false;  // macOS .app
        std::string team;
        std::string signingIdentity;
        std::string provisioningProfile;

        // Bundles to package (empty => cook-config distribution default).
        std::vector<std::string>                   bundles;
        std::vector<std::string>                   shaderTargets;
        std::vector<ExportPresetShaderEnvironment> shaderEnvironments;

        // Android.
        std::vector<std::string> abis;
        std::string              keystore;
        int                      compileSdk = 34;
        int                      minSdk     = 29;
        int                      targetSdk  = 33;
        std::string              ndkVersion = "28.1.13356709";

        // Runtime module packaging: dynamic | static (static compiles selected
        // modules into the host via a generated registration).
        std::string packing = "dynamic";

        // CMake build escape hatches (configure-time). `cacheVariables` is a JSON
        // object on disk, an ordered sequence in memory (see ExportPresetCacheVariable).
        std::vector<ExportPresetCacheVariable> cacheVariables;
        std::vector<std::string>               compileDefinitions;
        std::vector<std::string>               compileFlags;
        std::vector<std::string>               extraConfigureArgs;
        std::vector<std::string>               extraBuildArgs;
    };

    struct ExportPresets {
        std::vector<ExportPreset> presets;

        const ExportPreset *Find(const std::string &name) const;

        bool Load(const std::string &path);
        bool Save(const std::string &path) const;
    };

    // Owned by SerializationContext construction, alongside the framework types.
    class SerializationContext;
    void RegisterExportPresetReflection(SerializationContext *context);

} // namespace sky
