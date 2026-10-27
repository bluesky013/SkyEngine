#include <core/archive/MemoryStreamArchive.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>
#include <framework/world/WorldStreaming.h>

#include <algorithm>

namespace sky {
    WorldStreaming::~WorldStreaming()
    {
        for (auto &[id, cell] : cells)
            Cancel(cell);
    }

    void WorldStreaming::Cancel(Cell &cell)
    {
        for (const auto &actor : cell.preparing)
            for (const auto &[type, component] : actor->GetComponents())
                component->CancelPreparation();
        cell.preparing.clear();
    }

    bool WorldStreaming::AddCell(const Uuid &id, std::vector<std::unique_ptr<Actor>> &definitions, std::string &error, WorldCellTarget initialTarget)
    {
        if (initialTarget > WorldCellTarget::Active || !id || cells.contains(id) || definitions.size() > 128 || cells.size() >= 4096) {
            error = "Cell requires a unique UUID, at most 128 actors and at most 4096 cells per manifest";
            return false;
        }
        std::set<Uuid> ids;
        for (const auto &actor : definitions) {
            if (!actor || actor->GetWorld() || !actor->GetUuid() || world.GetActorByUuid(actor->GetUuid()) || actorIds.contains(actor->GetUuid()) ||
                !ids.insert(actor->GetUuid()).second) {
                error = "Cell actor identities must be unique across the entire world manifest";
                return false;
            }
        }
        // Cross-cell transform parents need an explicit residency group; do not resolve them opportunistically.
        for (const auto &actor : definitions) {
            if (auto *transform = actor->GetComponent<TransformComponent>();
                transform && transform->GetData().parent && !ids.contains(transform->GetData().parent)) {
                error = "A cell must include all transform ancestors";
                return false;
            }
        }
        for (const auto &actor : definitions) {
            auto          *current = actor.get();
            std::set<Uuid> ancestors;
            while (current) {
                if (!ancestors.insert(current->GetUuid()).second) {
                    error = "Cell transform hierarchy contains a cycle";
                    return false;
                }
                const auto *transform = current->GetComponent<TransformComponent>();
                const auto  parent    = transform ? transform->GetData().parent : Uuid{};
                const auto  found =
                    std::find_if(definitions.begin(), definitions.end(), [&](const auto &entry) { return entry->GetUuid() == parent; });
                current = found == definitions.end() ? nullptr : found->get();
            }
        }
        actorIds.insert(ids.begin(), ids.end());
        cells[id].definitions = std::move(definitions);
        cells[id].target = cells[id].initialTarget = initialTarget;
        return true;
    }

    bool WorldStreaming::Request(const Uuid &id, WorldCellTarget target)
    {
        auto found = cells.find(id);
        if (found == cells.end() || target > WorldCellTarget::Active)
            return false;
        auto &cell  = found->second;
        cell.target = target;
        if (cell.state == WorldCellState::Failed) {
            Cancel(cell);
            cell.state = WorldCellState::Unloaded;
            cell.error.clear();
        }
        return true;
    }

    WorldCellState WorldStreaming::GetState(const Uuid &id) const
    {
        const auto found = cells.find(id);
        return found == cells.end() ? WorldCellState::Unloaded : found->second.state;
    }
    std::string WorldStreaming::GetError(const Uuid &id) const
    {
        const auto found = cells.find(id);
        return found == cells.end() ? "Unknown cell" : found->second.error;
    }
    bool WorldStreaming::OwnsActor(const Uuid &id) const
    {
        return actorIds.contains(id);
    }

    bool WorldStreaming::Prepare(Cell &cell)
    {
        if (cell.state == WorldCellState::Unloaded || cell.preparing.size() < cell.definitions.size()) {
            cell.state     = WorldCellState::Preparing;
            const auto end = std::min(cell.definitions.size(), cell.preparing.size() + 8);
            for (size_t index = cell.preparing.size(); index < end; ++index) {
                const auto    &source = cell.definitions[index];
                OMemoryArchive memory;
                {
                    JsonOutputArchive output(memory);
                    source->SaveJson(output);
                }
                IMemoryArchive   stream(memory.Data(), memory.Size());
                JsonInputArchive input(stream);
                auto             actor = std::make_unique<Actor>();
                if (!actor->LoadJson(input, &cell.error))
                    return false;
                cell.preparing.push_back(std::move(actor));
            }
            if (cell.preparing.size() < cell.definitions.size())
                return true;
            for (const auto &actor : cell.preparing) {
                auto *transform = actor->GetComponent<TransformComponent>();
                if (!transform || !transform->GetData().parent)
                    continue;
                auto parent = std::find_if(cell.preparing.begin(), cell.preparing.end(),
                                           [&](const auto &entry) { return entry->GetUuid() == transform->GetData().parent; });
                if (parent == cell.preparing.end() || !(*parent)->GetComponent<TransformComponent>()) {
                    cell.error = "Cell transform parent is unavailable";
                    return false;
                }
                transform->SetParentPreserveLocal((*parent)->GetComponent<TransformComponent>());
            }
            cell.state = WorldCellState::Preparing;
        }
        for (const auto &actor : cell.preparing) {
            for (const auto &[type, component] : actor->GetComponents()) {
                const auto state = component->PrepareForWorld(world, cell.error);
                if (state == ComponentPreparation::Failed)
                    return false;
                if (state == ComponentPreparation::Preparing)
                    return true;
            }
        }
        cell.state = WorldCellState::Ready;
        return true;
    }

    bool WorldStreaming::Retire(Cell &cell)
    {
        std::vector<Actor *> retiring, promoted;
        for (const auto id : cell.active) {
            auto *actor = world.GetActorByUuid(id);
            if (!actor)
                continue;
            const bool persistent = std::any_of(actor->GetComponents().begin(), actor->GetComponents().end(),
                                                [](const auto &entry) { return entry.second->KeepWorldResidency(); });
            (persistent ? promoted : retiring).push_back(actor);
        }
        // Promote a whole transform tree so source-cell retirement cannot orphan persistent actors.
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto it = retiring.begin(); it != retiring.end();) {
                auto      *transform = (*it)->GetComponent<TransformComponent>();
                const bool related   = transform && std::any_of(promoted.begin(), promoted.end(), [&](auto *actor) {
                                         auto *other = actor->GetComponent<TransformComponent>();
                                         return other && (other->GetParent() == transform || transform->GetParent() == other);
                                     });
                if (related) {
                    promoted.push_back(*it);
                    it      = retiring.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }
        }
        if (!world.CanDetachGroup(retiring)) {
            cell.error = "Cell retirement is pinned by a constraint or physics support dependency";
            return false;
        }
        for (auto *actor : promoted) {
            actorIds.erase(actor->GetUuid());
            std::erase_if(cell.definitions, [actor](const auto &entry) { return entry->GetUuid() == actor->GetUuid(); });
        }
        if (!world.DetachGroup(retiring))
            return false;
        cell.active.clear();
        cell.error.clear();
        cell.state = WorldCellState::Unloaded;
        return true;
    }

    void WorldStreaming::Tick()
    {
        uint32_t preparing = 0, commits = 0;
        for (auto &[id, cell] : cells) {
            if (cell.target == WorldCellTarget::Unloaded) {
                if (cell.state == WorldCellState::Active || cell.state == WorldCellState::Retiring) {
                    cell.state = WorldCellState::Retiring;
                    if (commits < 2 && Retire(cell))
                        ++commits;
                } else {
                    Cancel(cell);
                    cell.state = WorldCellState::Unloaded;
                }
                continue;
            }
            if (cell.target == WorldCellTarget::Ready && (cell.state == WorldCellState::Active || cell.state == WorldCellState::Retiring)) {
                cell.state = WorldCellState::Retiring;
                if (commits < 2 && Retire(cell))
                    ++commits;
                continue;
            }
            if (cell.state == WorldCellState::Retiring) {
                cell.state = WorldCellState::Active;
                cell.error.clear();
            }
            if (cell.state == WorldCellState::Active || cell.state == WorldCellState::Failed)
                continue;
            if (cell.state != WorldCellState::Ready && preparing++ < 4 && !Prepare(cell)) {
                Cancel(cell);
                cell.state = WorldCellState::Failed;
            }
            if (cell.state == WorldCellState::Ready && cell.target == WorldCellTarget::Active && commits < 2) {
                std::vector<Uuid> ids;
                for (const auto &actor : cell.preparing)
                    ids.push_back(actor->GetUuid());
                if (world.TryAttachGroup(cell.preparing)) {
                    cell.active = std::move(ids);
                    cell.state  = WorldCellState::Active;
                } else {
                    cell.error = world.GetLastError();
                    Cancel(cell);
                    cell.state = WorldCellState::Failed;
                }
                ++commits;
            }
        }
    }

    void WorldStreaming::CollectAssetReferences(std::vector<ComponentAssetReference> &out) const
    {
        for (const auto &[id, cell] : cells)
            for (const auto &actor : cell.definitions)
                for (const auto &[type, component] : actor->GetComponents())
                    component->CollectAssetReferences(out);
    }

    void WorldStreaming::SaveJson(JsonOutputArchive &archive) const
    {
        archive.StartArray();
        for (const auto &[id, cell] : cells) {
            archive.StartObject();
            archive.Key("uuid");
            archive.SaveValue(id.ToString());
            archive.Key("initialTarget");
            archive.SaveEnum(cell.initialTarget);
            archive.Key("actors");
            archive.StartArray();
            for (const auto &actor : cell.definitions) {
                auto *active = world.GetActorByUuid(actor->GetUuid());
                (active ? active : actor.get())->SaveJson(archive);
            }
            archive.EndArray();
            archive.EndObject();
        }
        archive.EndArray();
    }

    bool WorldStreaming::LoadJson(JsonInputArchive &archive, std::string &error)
    {
        const rapidjson::Value *manifest = nullptr;
        archive.ForEachMember([&](const std::string &name, const rapidjson::Value &value) {
            if (name == "cells")
                manifest = &value;
        });
        const auto has = [](const rapidjson::Value &value, const char *name, rapidjson::Type type) {
            return value.IsObject() && value.HasMember(name) && value[name].GetType() == type;
        };
        if (!manifest || !manifest->IsArray() || manifest->Size() > 4096) {
            error = "World cells must be a bounded array";
            return false;
        }
        for (const auto &cell : manifest->GetArray()) {
            if (!has(cell, "uuid", rapidjson::kStringType) || !has(cell, "actors", rapidjson::kArrayType) || cell["actors"].Size() > 128 ||
                (cell.HasMember("initialTarget") && (!cell["initialTarget"].IsUint() || cell["initialTarget"].GetUint() > 2))) {
                error = "Malformed cell identity, actors or initial target";
                return false;
            }
            for (const auto &actor : cell["actors"].GetArray()) {
                if (!has(actor, "uuid", rapidjson::kStringType) || !has(actor, "name", rapidjson::kStringType) ||
                    !has(actor, "components", rapidjson::kArrayType)) {
                    error = "Malformed cell actor record";
                    return false;
                }
                for (const auto &component : actor["components"].GetArray()) {
                    if (!has(component, "type", rapidjson::kStringType) || !has(component, "data", rapidjson::kObjectType)) {
                        error = "Malformed cell component record";
                        return false;
                    }
                }
            }
        }
        const auto count = archive.StartArray("cells");
        if (count > 4096) {
            error = "World cell manifest exceeds cell budget";
            archive.End();
            return false;
        }
        for (uint32_t i = 0; i < count; ++i) {
            if (!archive.Start("uuid")) {
                error = "Cell identity is missing";
                archive.End();
                return false;
            }
            const auto id = Uuid::CreateFromString(archive.LoadString());
            archive.End();
            auto initialTarget = WorldCellTarget::Unloaded;
            if (archive.Start("initialTarget")) {
                initialTarget = static_cast<WorldCellTarget>(archive.LoadUint());
                archive.End();
            }
            std::vector<std::unique_ptr<Actor>> definitions;
            if (!archive.HasMember("actors")) {
                error = "Cell actor definitions are missing";
                archive.End();
                return false;
            }
            const auto actors = archive.StartArray("actors");
            if (actors > 128) {
                error = "Cell activation group exceeds actor budget";
                archive.End();
                archive.End();
                return false;
            }
            for (uint32_t j = 0; j < actors; ++j) {
                auto actor = std::make_unique<Actor>();
                if (!actor->LoadJson(archive, &error)) {
                    archive.End();
                    archive.End();
                    return false;
                }
                definitions.push_back(std::move(actor));
                archive.NextArrayElement();
            }
            archive.End();
            if (!AddCell(id, definitions, error, initialTarget)) {
                archive.End();
                return false;
            }
            archive.NextArrayElement();
        }
        archive.End();
        return true;
    }
} // namespace sky
