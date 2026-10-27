#pragma once

#include <framework/world/Actor.h>
#include <map>
#include <set>

namespace sky {
    enum class WorldCellState : uint8_t { Unloaded, Preparing, Ready, Active, Retiring, Failed };
    enum class WorldCellTarget : uint8_t { Unloaded, Ready, Active };

    // Host-thread controller. Spatial policy chooses targets; Tick applies them before world simulation.
    class WorldStreaming {
    public:
        explicit WorldStreaming(World &world) : world(world)
        {
        }
        ~WorldStreaming();
        bool           AddCell(const Uuid                          &id,
                               std::vector<std::unique_ptr<Actor>> &definitions,
                               std::string                         &error,
                               WorldCellTarget                      initialTarget = WorldCellTarget::Unloaded);
        bool           Request(const Uuid &id, WorldCellTarget target);
        WorldCellState GetState(const Uuid &id) const;
        std::string    GetError(const Uuid &id) const;
        bool           OwnsActor(const Uuid &id) const;
        void           Tick();
        void           SaveJson(JsonOutputArchive &archive) const;
        bool           LoadJson(JsonInputArchive &archive, std::string &error);
        void           CollectAssetReferences(std::vector<ComponentAssetReference> &out) const;

    private:
        struct Cell {
            WorldCellTarget                     initialTarget = WorldCellTarget::Unloaded;
            WorldCellState                      state         = WorldCellState::Unloaded;
            WorldCellTarget                     target        = WorldCellTarget::Unloaded;
            std::vector<std::unique_ptr<Actor>> definitions;
            std::vector<std::unique_ptr<Actor>> preparing;
            std::vector<Uuid>                   active;
            std::string                         error;
        };
        bool                 Prepare(Cell &cell);
        bool                 Retire(Cell &cell);
        void                 Cancel(Cell &cell);
        World               &world;
        std::map<Uuid, Cell> cells;
        std::set<Uuid>       actorIds;
    };
} // namespace sky
