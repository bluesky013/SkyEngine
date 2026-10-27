#pragma once

#include <core/environment/Singleton.h>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace sky {
    class Actor;
}

namespace sky::editor {
    struct ActorCreation {
        std::shared_ptr<const void> moduleLease;
        std::string                 id;
        std::string                 label;
        std::string                 group;
        int                         order = 0;
        // Prepare a detached actor; failure discards it without editing the world.
        std::function<bool(Actor &)> create;
    };

    class EditorActorCreation : public Singleton<EditorActorCreation> {
    public:
        bool                                 RegisterCreation(ActorCreation creation);
        bool                                 UnRegisterCreation(const std::string &id);
        std::shared_ptr<const ActorCreation> Find(const std::string &id) const;
        const auto                          &GetTools() const
        {
            return creators;
        }
        uint64_t GetRevision() const
        {
            return revision;
        }

    private:
        std::map<std::string, std::shared_ptr<const ActorCreation>> creators;
        uint64_t                                                    revision = 0;
    };
} // namespace sky::editor
