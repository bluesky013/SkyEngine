//
// Created on 2026/10/07.
//

#include <editor/core/document/WorldDocument.h>

#include <framework/serialization/JsonArchive.h>
#include <framework/world/WorldDesc.h>
#include <framework/world/WorldSubSystemRegistry.h>

#include <core/archive/StreamArchive.h>
#include <core/file/FileSystem.h>

#include <sstream>

namespace sky::editor {

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
            return false;
        }
        sky::JsonInputArchive json(*archive);
        world->LoadJson(json);

        if (const sky::WorldDesc *desc = world->GetWorldDesc(); desc != nullptr) {
            world->Build(*desc);
        }
        ClearDirty();
        return true;
    }

    bool WorldDocument::Save()
    {
        const sky::FilePath filePath(GetPath());
        sky::NativeFile     file(filePath);
        auto                archive = file.WriteAsArchive();
        if (!archive || !archive->IsOpen()) {
            return false;
        }
        sky::JsonOutputArchive json(*archive);
        world->SaveJson(json);
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
        desc->subSystems.push_back(std::move(entry));
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

    bool WorldDocument::Rebuild()
    {
        const sky::WorldDesc *desc = world->GetWorldDesc();
        if (desc == nullptr) {
            return false;
        }
        world->Build(*desc);
        return true;
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
            play->LoadJson(in);
        }
        if (const sky::WorldDesc *desc = play->GetWorldDesc(); desc != nullptr) {
            play->Build(*desc);
        }
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
                    MarkDirty();
                }
                return &entry.config;
            }
        }

        sky::WorldSubSystemDesc entry;
        entry.name = key;
        if (reg->makeDefaultConfig) {
            entry.config = reg->makeDefaultConfig();
        }
        desc->subSystems.push_back(std::move(entry));
        MarkDirty();
        return &desc->subSystems.back().config;
    }

} // namespace sky::editor
