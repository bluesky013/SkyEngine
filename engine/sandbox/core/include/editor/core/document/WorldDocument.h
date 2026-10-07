//
// Created on 2026/10/07.
//

#pragma once

#include <editor/core/document/Document.h>
#include <framework/serialization/Any.h>
#include <framework/world/World.h>

#include <string>

namespace sky {
    struct WorldDesc;
} // namespace sky

namespace sky::editor {

    // The Sandbox editor's project-world document: owns a sky::World and loads/saves
    // it through the framework JSON archive. The mutable world description backs the
    // project-level world subsystem configuration surface.
    class WorldDocument : public Document {
    public:
        explicit WorldDocument(std::string path);
        ~WorldDocument() override;

        bool Load() override;
        bool Save() override;

        sky::World *GetWorld() const
        {
            return world.Get();
        }
        const sky::WorldDesc *GetDesc() const;

        // Config surface: enable/disable a registered subsystem (adds an entry if absent).
        bool SetSubSystemEnabled(const std::string &name, bool enabled);
        bool IsSubSystemEnabled(const std::string &name, bool &enabled) const;

        // (Re)builds the world's subsystems from its description. Adds enabled,
        // missing subsystems; removal is not supported yet.
        bool Rebuild();

        // Finds (or creates, with the registry's default) the stored config for a
        // subsystem so a reflected form can edit it in place. Returns null when the
        // subsystem is not registered. Marks the document dirty.
        Any *EnsureSubSystemConfig(const std::string &name);

        // Play-In-Editor duplication: builds a fresh world from the edit world's
        // serialized content (actors/components + WorldDesc), creates its subsystems
        // and starts their simulation. The edit world is never touched.
        sky::WorldPtr CreatePlayWorld();

    private:
        sky::WorldPtr world;
    };

} // namespace sky::editor
