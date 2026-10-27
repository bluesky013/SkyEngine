//
// Created on 2026/10/07.
//

#pragma once

#include <editor/core/command/UndoCommand.h>
#include <editor/core/document/Document.h>
#include <framework/serialization/Any.h>
#include <framework/world/World.h>

#include <string>

namespace sky {
    struct WorldDesc;
} // namespace sky

namespace sky::editor {
    class PropertyDescriptor;

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
        bool     SetSubSystemEnabled(const std::string &name, bool enabled);
        bool     RemoveSubSystem(const std::string &name);
        uint64_t GetConfigRevision() const
        {
            return configRevision;
        }
        bool IsSubSystemEnabled(const std::string &name, bool &enabled) const;

        const std::string &GetLastError() const
        {
            return lastError;
        }

        // Finds (or creates, with the registry's default) the stored config for a
        // subsystem so a reflected form can edit it in place. Returns null when the
        // subsystem is not registered. Marks the document dirty.
        Any           *EnsureSubSystemConfig(const std::string &name);
        UndoCommandPtr MakeConfigEditCommand(const std::string &name, const PropertyDescriptor &property, Any value);
        UndoCommandPtr MakeSubSystemEnabledCommand(const std::string &name, bool enabled);

        // Builds a validated runtime snapshot without changing the authoring world.
        // PlaySession owns StartSimulation/StopSimulation.
        sky::WorldPtr CreatePlayWorld();

    private:
        sky::WorldPtr         world;
        std::string           lastError;
        uint64_t              configRevision = 0;
        std::shared_ptr<void> editLifetime   = std::make_shared<int>(0);
    };

} // namespace sky::editor
