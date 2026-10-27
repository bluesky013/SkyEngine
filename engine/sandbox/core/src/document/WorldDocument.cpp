//
// Created on 2026/10/07.
//

#include <editor/core/command/PropertyEditCommand.h>
#include <editor/core/document/WorldDocument.h>
#include <editor/core/property/PropertyDescriptor.h>

#include <framework/serialization/JsonArchive.h>
#include <framework/world/WorldDesc.h>
#include <framework/world/WorldSubSystemRegistry.h>

#include <core/archive/StreamArchive.h>
#include <core/file/FileSystem.h>

#include <sstream>

namespace sky::editor {
    namespace {
        class ConfigEditCommand final : public PropertyEditCommand {
        public:
            ConfigEditCommand(std::function<void *()> resolver, const serialize::TypeMemberNode *member, Any value, WorldDocument &document)
                : PropertyEditCommand(std::move(resolver), member, std::move(value), "Edit world configuration"), document(document)
            {
            }
            bool TryDo() override
            {
                if (!PropertyEditCommand::TryDo())
                    return false;
                document.MarkDirty();
                return true;
            }
            bool TryUndo() override
            {
                if (!PropertyEditCommand::TryUndo())
                    return false;
                document.MarkDirty();
                return true;
            }

        private:
            WorldDocument &document;
        };

        class SubSystemEnabledCommand final : public UndoCommand {
        public:
            SubSystemEnabledCommand(WorldDocument &document, std::weak_ptr<void> lifetime, std::string name, bool enabled)
                : UndoCommand("Enable world subsystem"), document(document), lifetime(std::move(lifetime)), name(std::move(name)), enabled(enabled)
            {
                existed = document.IsSubSystemEnabled(this->name, previous);
            }
            void Do() override
            {
                TryDo();
            }
            void Undo() override
            {
                TryUndo();
            }
            bool TryDo() override
            {
                return !lifetime.expired() && document.SetSubSystemEnabled(name, enabled);
            }
            bool TryUndo() override
            {
                if (lifetime.expired())
                    return false;
                if (existed)
                    return document.SetSubSystemEnabled(name, previous);
                return document.RemoveSubSystem(name);
            }

        private:
            WorldDocument      &document;
            std::weak_ptr<void> lifetime;
            std::string         name;
            bool                enabled;
            bool                previous = false;
            bool                existed  = false;
        };
    } // namespace

    WorldDocument::WorldDocument(std::string path) : Document(std::move(path))
    {
        world = sky::World::CreateWorld();
        world->Init();
    }

    WorldDocument::~WorldDocument() = default;

    bool WorldDocument::Load()
    {
        const sky::FilePath filePath(GetPath());
        sky::NativeFile     file(filePath);
        auto                archive = file.ReadAsArchive();
        if (!archive || !archive->IsOpen()) {
            lastError = "Cannot read World file: " + GetPath();
            return false;
        }
        sky::WorldPtr         candidate = sky::World::CreateWorld();
        sky::JsonInputArchive json(*archive);
        if (!candidate->LoadJson(json) || !candidate->Build(*candidate->GetMutableWorldDesc())) {
            lastError = candidate->GetLastError();
            return false;
        }
        world = std::move(candidate);
        ++configRevision;
        editLifetime = std::make_shared<int>(0);
        lastError.clear();
        ClearDirty();
        return true;
    }

    bool WorldDocument::Save()
    {
        const sky::FilePath filePath(GetPath());
        sky::NativeFile     file(filePath);
        auto                archive = file.WriteAsArchive();
        if (!archive || !archive->IsOpen()) {
            lastError = "Cannot write World file: " + GetPath();
            return false;
        }
        sky::JsonOutputArchive json(*archive);
        world->SaveJson(json);
        lastError.clear();
        ClearDirty();
        return true;
    }

    const sky::WorldDesc *WorldDocument::GetDesc() const
    {
        return world->GetWorldDesc();
    }

    bool WorldDocument::SetSubSystemEnabled(const std::string &name, bool enabled)
    {
        sky::WorldDesc *desc = world->GetMutableWorldDesc();
        for (auto &entry : desc->subSystems) {
            if (entry.name == name.c_str()) {
                entry.enabled = enabled;
                MarkDirty();
                return true;
            }
        }
        sky::WorldSubSystemDesc entry;
        entry.name    = sky::Name(name.c_str());
        entry.enabled = enabled;
        if (const auto *registration = WorldSubSystemRegistry::Get().GetRegistration(entry.name); registration && registration->makeDefaultConfig) {
            entry.config = registration->makeDefaultConfig();
        }
        desc->subSystems.push_back(std::move(entry));
        ++configRevision;
        MarkDirty();
        return true;
    }

    bool WorldDocument::RemoveSubSystem(const std::string &name)
    {
        if (std::erase_if(world->GetMutableWorldDesc()->subSystems, [&](const auto &entry) { return entry.name == name.c_str(); }) == 0)
            return false;
        ++configRevision;
        MarkDirty();
        return true;
    }

    bool WorldDocument::IsSubSystemEnabled(const std::string &name, bool &enabled) const
    {
        const sky::WorldDesc *desc = world->GetWorldDesc();
        if (desc == nullptr) {
            return false;
        }
        for (const auto &entry : desc->subSystems) {
            if (entry.name == name.c_str()) {
                enabled = entry.enabled;
                return true;
            }
        }
        return false;
    }

    sky::WorldPtr WorldDocument::CreatePlayWorld()
    {
        if (world == nullptr) {
            return nullptr;
        }

        // Duplicate by serializing the edit world and loading it into a fresh one.
        std::stringstream  stream;
        sky::StreamArchive archive(stream);
        {
            sky::JsonOutputArchive out(archive);
            world->SaveJson(out);
        }
        stream.seekg(0);

        auto play = sky::CounterPtr<sky::World>(sky::World::CreateWorld());
        play->Init();
        {
            sky::JsonInputArchive in(archive);
            if (!play->LoadJson(in)) {
                lastError = play->GetLastError();
                return nullptr;
            }
        }
        if (!play->Build(*play->GetMutableWorldDesc())) {
            lastError = play->GetLastError();
            return nullptr;
        }
        lastError.clear();
        // Simulation lifecycle (StartSimulation/StopSimulation) is owned by the
        // play session, not by duplication.
        return play;
    }

    Any *WorldDocument::EnsureSubSystemConfig(const std::string &name)
    {
        const Name                             key(name.c_str());
        const sky::WorldSubSystemRegistration *reg = sky::WorldSubSystemRegistry::Get().GetRegistration(key);
        if (reg == nullptr) {
            return nullptr;
        }

        sky::WorldDesc *desc = world->GetMutableWorldDesc();
        for (auto &entry : desc->subSystems) {
            if (entry.name == key) {
                if (!entry.config && reg->makeDefaultConfig) {
                    entry.config = reg->makeDefaultConfig();
                    ++configRevision;
                    MarkDirty();
                }
                return &entry.config;
            }
        }

        sky::WorldSubSystemDesc entry;
        entry.name    = key;
        entry.enabled = false;
        if (reg->makeDefaultConfig) {
            entry.config = reg->makeDefaultConfig();
        }
        desc->subSystems.push_back(std::move(entry));
        ++configRevision;
        MarkDirty();
        return &desc->subSystems.back().config;
    }

    UndoCommandPtr WorldDocument::MakeConfigEditCommand(const std::string &name, const PropertyDescriptor &property, Any value)
    {
        const auto *member       = property.GetMember();
        const auto *registration = WorldSubSystemRegistry::Get().GetRegistration(Name(name.c_str()));
        if (!member || !registration || !registration->configType)
            return {};
        const auto  type = registration->configType->registeredId;
        const auto *node = GetTypeNode(type);
        if (!node || std::none_of(node->members.begin(), node->members.end(), [&](const auto &field) { return &field.second == member; }))
            return {};
        const std::weak_ptr<void> lifetime = editLifetime;
        auto                      resolve  = [lifetime, world = world, name, type]() -> void                       *{
            if (lifetime.expired())
                return nullptr;
            for (auto &entry : world->GetMutableWorldDesc()->subSystems) {
                if (entry.name == name.c_str() && entry.config.Info() && entry.config.Info()->registeredId == type)
                    return entry.config.Data();
            }
            return nullptr;
        };
        return std::make_unique<ConfigEditCommand>(std::move(resolve), member, std::move(value), *this);
    }

    UndoCommandPtr WorldDocument::MakeSubSystemEnabledCommand(const std::string &name, bool enabled)
    {
        if (!WorldSubSystemRegistry::Get().IsRegistered(Name(name.c_str())))
            return {};
        return std::make_unique<SubSystemEnabledCommand>(*this, editLifetime, name, enabled);
    }

} // namespace sky::editor
