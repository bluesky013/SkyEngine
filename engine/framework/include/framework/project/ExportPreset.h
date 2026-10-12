//
// Created on 2026/10/12.
//

#pragma once

#include <string>
#include <vector>

namespace sky {

    // One packaging distribution (an entry of `export_presets.json`). The same
    // schema is consumed by python/package_project.py and edited in the editor
    // through the generic reflection panel.
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
        std::vector<std::string> bundles;

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
    };

    struct ExportPresets {
        std::vector<ExportPreset> presets;

        const ExportPreset *Find(const std::string &name) const;

        bool Load(const std::string &path);
        bool Save(const std::string &path) const;
    };

    // Registers `ExportPreset` with the reflection/serialization context (idempotent).
    void RegisterExportPresetReflection();

} // namespace sky
