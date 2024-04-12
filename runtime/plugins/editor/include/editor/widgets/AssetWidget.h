//
// Created by blues on 2024/3/31.
//

#pragma once

#include <imgui/ImWidget.h>
#include <editor/event/Event.h>
#include <core/util/Uuid.h>
#include <array>

namespace sky::editor {

    class AssetWidget : public ImWidget, public IToggleEvent  {
    public:
        AssetWidget() : ImWidget("Asset") {}
        ~AssetWidget() override = default;

        void Execute(ImContext &context) override;
        void BindEvent(EventID id);

    private:
        void OnToggle(bool val) override { show = val; }
        static const uint32_t MAX_INPUT_LENGTH = 128;

        bool show = false;
        std::array<char, MAX_INPUT_LENGTH> inputText = {};

        Uuid selectedId;
        EventBinder<IToggleEvent> binder;
    };

} // namespace sky::editor