//
// Created on 2026/10/05.
//

#pragma once

#include <core/environment/Singleton.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky {

    // Engine version used for project association / validation.
    inline constexpr const char *kEngineVersion = "0.1.0";

    // Project descriptor (`*.skyproj`). The single source of project identity and
    // settings; the editor/launcher read it to bind a project. Packaging identity
    // (bundle id, versions) lives here; per-platform export options and signing
    // credentials live in `export_presets.json` / `export_credentials.json`
    // (consumed by python/package_project.py), not in the descriptor.
    struct ProjectDescriptor {
        std::string              id;
        std::string              name;
        std::string              engineVersion;
        std::string              defaultScene;
        std::string              rhi = "vulkan";      // settings.rhi: preferred RHI backend (vulkan/metal/dx12)
        std::string              applicationId;       // settings.applicationId: default reverse-DNS id (Android/Apple)
        std::string              versionName = "1.0"; // settings.versionName
        int                      versionCode = 1;     // settings.versionCode
        std::vector<std::string> modules;             // module ids
        std::vector<std::string> plugins;             // plugin ids
        std::vector<std::string> resourceModules;
        std::string              filePath; // the .skyproj path (runtime only, not serialized)

        std::string Dir() const;        // project directory (parent of filePath)
        std::string AssetsDir() const;  // <dir>/assets
        std::string ConfigsDir() const; // <dir>/configs
        std::string CacheDir() const;   // <dir>/cache

        // Resolved application id: the configured value, or a default derived from
        // the project name ("com.skyengine.<identifier>") when unset.
        std::string ResolvedApplicationId() const;

        bool Read(const std::string &skyprojPath);
        bool Write(const std::string &skyprojPath) const;

        // Project creation is implemented by the packaging CLI
        // (python/package_project.py create); the descriptor is written by it.
    };

    // Recent-projects list, persisted under the user config path. Runtime
    // counterpart of the old python `project_manager.ini`.
    class ProjectRegistry : public Singleton<ProjectRegistry> {
    public:
        void                            Load();
        void                            Save();
        void                            Add(const std::string &skyprojPath); // most-recent first, deduped
        void                            Remove(const std::string &skyprojPath);
        const std::vector<std::string> &Recent() const
        {
            return recent;
        }
        std::string FilePath() const;

    private:
        std::vector<std::string> recent; // .skyproj paths
    };

} // namespace sky
