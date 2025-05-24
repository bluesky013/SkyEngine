//
// Created by blues on 2025/5/18.
//

#pragma once

#include <editor/framework/EditorActorCreation.h>

namespace sky::editor {

    class RenderCubeActorCreator : public IActorCreateBase {
    public:
        RenderCubeActorCreator() : group("Graphics"), name("Cube") {}
        ~RenderCubeActorCreator() override = default;

        Name GetGroup() const override { return group; }
        Name GetName() const override { return name; }

        bool OnCreateActor(Actor* actor) override;

    private:
        Name group;
        Name name;
    };

} // namespace sky::editor
