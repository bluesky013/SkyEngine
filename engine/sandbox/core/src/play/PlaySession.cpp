//
// Created on 2026/10/07.
//

#include <editor/core/play/PlaySession.h>

namespace sky::editor {

    bool PlaySession::Play()
    {
        if (state == PlayState::Paused) {
            state = PlayState::Playing;
            return true;
        }
        if (state == PlayState::Playing) {
            return false;
        }

        sky::WorldPtr runtime = factory ? factory() : nullptr;
        if (runtime == nullptr) {
            return false;
        }
        world = std::move(runtime);
        world->StartSimulation();
        time  = 0.0f;
        state = PlayState::Playing;
        return true;
    }

    bool PlaySession::Pause()
    {
        if (state != PlayState::Playing) {
            return false;
        }
        state = PlayState::Paused;
        return true;
    }

    bool PlaySession::Stop()
    {
        if (state == PlayState::Editing) {
            return false;
        }
        if (world != nullptr) {
            world->StopSimulation();
        }
        world = nullptr;
        time  = 0.0f;
        state = PlayState::Editing;
        return true;
    }

    void PlaySession::Tick(float delta)
    {
        if (state != PlayState::Playing || world == nullptr) {
            return;
        }
        time += delta;
        world->Tick(delta);
    }

} // namespace sky::editor
