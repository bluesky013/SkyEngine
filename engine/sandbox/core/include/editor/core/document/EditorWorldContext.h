//
// Created on 2026/10/30.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/world/World.h>

namespace sky::editor {

    // Published by the host on the main thread; plugins never retain a document pointer.
    class EditorWorldContext : public Singleton<EditorWorldContext> {
        friend class Singleton<EditorWorldContext>;

    public:
        void SetWorld(World *value, bool allowEditing)
        {
            world    = value;
            editable = allowEditing;
        }
        World *GetWorld() const
        {
            return world.Get();
        }
        bool IsEditable() const
        {
            return editable && world != nullptr;
        }
        void Clear()
        {
            SetWorld(nullptr, false);
        }

    private:
        WorldPtr world;
        bool     editable = false;
    };

} // namespace sky::editor
