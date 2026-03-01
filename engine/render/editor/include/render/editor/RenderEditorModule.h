//
// Created by blues on 2025/5/18.
//

#pragma once

#include <render/adaptor/RenderModule.h>
#include <render/geometry/GeometryFactory.h>
#include <render/editor/RenderEditorActorCreation.h>
#include <editor/framework/EditorActorCreation.h>

namespace sky::editor {

    class RenderEditorModule : public RenderModule {
    public:
        RenderEditorModule() = default;
        ~RenderEditorModule() override = default;

        bool Init(const StartArguments &args) override;
        void Shutdown() override;

    private:
        template <typename T>
        void RegisterActorCreators(BuiltinGeometryType type)
        {
            auto iter = actorFuncs.emplace(type, std::make_unique<T>());

            EditorActorCreation::Get()->RegisterCreation(iter.first->second.get());
        }

        std::unordered_map<BuiltinGeometryType, std::unique_ptr<IActorCreateBase>> actorFuncs;
    };

} // namespace sky::editor
