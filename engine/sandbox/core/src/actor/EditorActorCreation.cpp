#include <editor/core/actor/EditorActorCreation.h>

namespace sky::editor {
    bool EditorActorCreation::RegisterCreation(ActorCreation creation)
    {
        if (creation.id.empty() || creation.label.empty() || !creation.create || creators.contains(creation.id))
            return false;
        auto entry = std::make_shared<const ActorCreation>(std::move(creation));
        creators.emplace(entry->id, entry);
        ++revision;
        return true;
    }
    bool EditorActorCreation::UnRegisterCreation(const std::string &id)
    {
        if (creators.erase(id) == 0)
            return false;
        ++revision;
        return true;
    }
    std::shared_ptr<const ActorCreation> EditorActorCreation::Find(const std::string &id) const
    {
        const auto found = creators.find(id);
        return found == creators.end() ? nullptr : found->second;
    }
} // namespace sky::editor
