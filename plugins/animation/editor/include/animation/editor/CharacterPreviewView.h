#pragma once

#include "animation/editor/CharacterPreview.h"
#include <editor/ui/AssetViewProvider.h>
#include <ui/UIElement.h>

namespace sky::animation::editor {
    void RegisterCharacterPreviewViewTypes();
    class CharacterPreviewActions {
    public:
        CharacterPreviewActions(std::shared_ptr<CharacterPreview> preview, std::function<void()> defaults, std::string scope = "assetViewer");
        ~CharacterPreviewActions();

    private:
        std::shared_ptr<void>    lifetime = std::make_shared<int>(0);
        std::vector<std::string> ids;
        std::string scope;
    };
    class CharacterPreviewPanel final : public sky::ui::UIElement {
    public:
        CharacterPreviewPanel(CharacterPreview &preview, sky::ui::UITextSystem *text);
        ~CharacterPreviewPanel() override;
        void                         Sync();
        void                         Cancel();
        bool                         Editing() const;
        void                         ApplyDefaults();
        sky::editor::CommandService &History();
        bool                         WantsPointerCapture() const override;
        sky::ui::UIEventResult       OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult       OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult       OnTextInput(const sky::ui::UITextInputEvent &event) override;
        void                         OnPointerCancel(uint32_t pointer) override;

    protected:
        sky::ui::UISize OnMeasure(const sky::ui::UIConstraints &constraints) override;
        void            ArrangeChildren(const sky::ui::UIRect &content) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
    std::unique_ptr<sky::ui::UIElement>              CreateCharacterViewport(CharacterPreview &preview, sky::ui::UITextSystem *text);
    std::unique_ptr<sky::editor::IAssetViewDocument> CreateCharacterEditor(std::shared_ptr<CharacterPreview> preview);
} // namespace sky::animation::editor
