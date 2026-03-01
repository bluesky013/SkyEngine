//
// Created by blues on 2024/4/6.
//

#pragma once

#include <imgui/ImWidget.h>
#include <editor/event/Event.h>

namespace sky::editor {

    class AssetInspector : public ImWidget, public IAssetViewEvent {
    public:
        AssetInspector() : ImWidget("Asset Inspector") {}
        ~AssetInspector() override = default;

        void Execute(ImContext &context) override;

    private:
        void OnClicked(const Uuid& type, void *data) override;

        bool show = false;
        Uuid currentType;
        void *currentData = nullptr;
    };

} // sky::editor