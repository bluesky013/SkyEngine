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
    // settings; the editor/launcher read it to bind a project. See the editor
    // framework design doc for the full field set (manifest entries land later).
    struct ProjectDescriptor {
        std::string id;
        std::string name;
        std::string engineVersion;
        std::string defaultScene;
        std::vector<std::string> modules; // module ids
        std::vector<std::string> plugins; // plugin ids
        std::string filePath;             // the .skyproj path (runtime only, not serialized)

        std::string Dir() const;         // project directory (parent of filePath)
        std::string AssetsDir() const;   // <dir>/assets
        std::string ConfigsDir() const;  // <dir>/configs
        std::string CacheDir() const;    // <dir>/cache

        bool Read(const std::string &skyprojPath);
        bool Write(const std::string &skyprojPath) const;

        // Creates the project directory layout (dirs + a default descriptor) and
        // returns true on success; `skyprojPath` is set to the written file.
        static bool Create(const std::string &dir, const std::string &name, std::string &skyprojPath);

        static std::string MakeId();
    };

    // Recent-projects list, persisted under the user config path. Runtime
    // counterpart of the old python `project_manager.ini`.
    class ProjectRegistry : public Singleton<ProjectRegistry> {
    public:
        void Load();
        void Save();
        void Add(const std::string &skyprojPath); // most-recent first, deduped
        void Remove(const std::string &skyprojPath);
        const std::vector<std::string> &Recent() const { return recent; }
        std::string FilePath() const;

    private:
        std::vector<std::string> recent; // .skyproj paths
    };

} // namespace sky
