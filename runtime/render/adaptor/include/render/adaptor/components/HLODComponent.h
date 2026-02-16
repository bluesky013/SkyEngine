//
// Created by Copilot on 2026/2/16.
//

#pragma once

#include <framework/world/Component.h>
#include <render/hlod/HLODTree.h>
#include <render/hlod/HLODRenderer.h>

namespace sky {

    class HLODComponent : public ComponentBase {
    public:
        HLODComponent() = default;
        ~HLODComponent() override;

        COMPONENT_RUNTIME_INFO(HLODComponent)

        static void Reflect(SerializationContext *context);

        void Tick(float time) override;

        void SaveJson(JsonOutputArchive &ar) const override;
        void LoadJson(JsonInputArchive &ar) override;

        void SetHLODTree(const HLODTreePtr &tree);
        HLODRenderer *GetRenderer() const { return renderer; }

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
    private:
        void ShutDown();
        void BuildRenderer();

        HLODTreePtr hlodTree;
        HLODRenderer *renderer = nullptr;
    };

} // namespace sky
