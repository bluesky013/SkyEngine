//
// Created on 2026/10/07.
//

#pragma once

#include <framework/world/World.h>

#include <functional>

namespace sky::editor {

    // Play-In-Editor session state.
    enum class PlayState {
        Editing = 0,
        Playing,
        Paused,
    };

    // Host-owned Play-In-Editor session. It owns the runtime world (duplicated from
    // the edit world) and advances it only while Playing. The edit world is never
    // ticked or mutated. UI-free so it can live in the editor core and be tested.
    class PlaySession {
    public:
        // Creates a fresh runtime world (the host wires this to
        // WorldDocument::CreatePlayWorld). Returns null when no world is available.
        using WorldFactory = std::function<sky::WorldPtr()>;

        void SetWorldFactory(WorldFactory factory)
        {
            this->factory = std::move(factory);
        }

        // Starts (or resumes) play. Returns false when the session cannot start
        // (no source world).
        bool Play();
        // Suspends the running session (keeps the runtime world alive).
        bool Pause();
        // Ends the session and discards the runtime world.
        bool Stop();

        // Advances the runtime world while Playing.
        void Tick(float delta);

        PlayState GetState() const
        {
            return state;
        }
        const sky::WorldPtr &GetWorld() const
        {
            return world;
        }
        // Accumulated simulation time (seconds).
        float GetTime() const
        {
            return time;
        }

    private:
        WorldFactory  factory;
        sky::WorldPtr world;
        PlayState     state = PlayState::Editing;
        float         time  = 0.0f;
    };

} // namespace sky::editor
