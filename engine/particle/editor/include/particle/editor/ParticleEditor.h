#pragma once

#include <editor/core/command/CommandService.h>
#include <editor/core/document/Document.h>
#include <editor/ui/AssetViewProvider.h>
#include <particle/ParticleEffect.h>
#include <particle/ParticleProgram.h>
#include <ui/UIElement.h>

#include <functional>
#include <memory>
#include <string>

namespace sky {
    class SerializationContext;
}
namespace sky::ui {
    class UITextSystem;
}
namespace sky::editor {
    class ReflectedFormView;
}
namespace sky::particle::editor {

    struct ParticleSourceStorage {
        std::function<bool(std::string &, std::string &)>    read;
        std::function<bool(std::string_view, std::string &)> write;
        std::function<void(const Uuid &)>                    requestCook;
    };
    ParticleSourceStorage ParticleEffectStorage(const Uuid &asset);

    class ParticleDocument final : public sky::editor::Document {
    public:
        explicit ParticleDocument(Uuid asset, ParticleSourceStorage storage);
        explicit ParticleDocument(Uuid asset);
        ~ParticleDocument() override;
        bool                         Load() override;
        bool                         Save() override;
        void                         RequestSave();
        bool                         FlushPendingSave();
        bool                         IsSavePending() const;
        bool                         IsLoaded() const;
        const ParticleEffectDesc    &Effect() const;
        ParticleEffectDesc          &Effect();
        const ParticleCompileResult &Compiled() const;
        const std::string           &LastError() const;
        uint64_t                     Revision() const;
        const Uuid                  &AssetId() const;
        sky::editor::CommandService &Commands();
        void                         MarkEdited();
        void                         ReplaceEffect(ParticleEffectDesc next);
        sky::editor::UndoCommandPtr  MakeEdit(std::string label, ParticleEffectDesc next);
        bool                         Apply(std::string label, ParticleEffectDesc next);

    private:
        class Edit;
        void                        SetEffect(const ParticleEffectDesc &next);
        Uuid                        asset;
        ParticleSourceStorage       storage;
        ParticleEffectDesc          effect;
        ParticleCompileResult       compiled;
        std::string                 saved;
        std::string                 lastError;
        uint64_t                    revision    = 0;
        bool                        savePending = false;
        bool                        loaded      = false;
        sky::editor::CommandService commands;
    };

    class ParticleEditorView final : public sky::ui::UIElement {
    public:
        ParticleEditorView(ParticleDocument &document, sky::ui::UITextSystem *text, std::string actionScope = {});
        ~ParticleEditorView() override;
        void                         OnPaint(sky::ui::UIPaintContext &context) override;
        void                         ArrangeChildren(const sky::ui::UIRect &content) override;
        sky::ui::UIEventResult       OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        bool                         WantsPointerCapture() const override;
        sky::ui::UIEventResult       OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult       OnTextInput(const sky::ui::UITextInputEvent &event) override;
        void                         OnPointerCancel(uint32_t pointer) override;
        void                         OnPointerLeave(const sky::ui::UIPointerEvent &event) override;
        void                         Tick(float delta);
        bool                         Editing() const;
        void                         Cancel();
        sky::editor::CommandService &History();
        const std::string           &Error() const;

    protected:
        sky::ui::UISize OnMeasure(const sky::ui::UIConstraints &constraints) override;

    private:
        void                            AddEmitter();
        void                            AddModule();
        void                            AddRenderer();
        void                            RemoveSelection();
        void                            MoveModule(int direction);
        ParticleDocument               &document;
        sky::ui::UITextSystem          *text = nullptr;
        sky::editor::ReflectedFormView *form = nullptr;
        class StackView;
        void                                    BindSelection();
        StackView                              *stack = nullptr;
        std::string                             error;
        std::string                             actionScope;
        ParticleEffectDesc                      editSnapshot;
        uint64_t                                boundRevision = 0;
        sky::editor::CommandService::CallbackId beforeHistory = 0;
        int                                     selectedEmitter   = -1;
        int                                     selectedModule    = -1;
        int                                     selectedRenderer  = -1;
        int                                     selectedParameter = -1;
    };

    std::unique_ptr<sky::editor::IAssetViewDocument> CreateParticleEditorDocument(const Uuid &asset, const std::string &type);
    std::unique_ptr<sky::editor::IAssetViewProvider> CreateParticleViewProvider();

    void RegisterParticleAssetViews(SerializationContext *context);
    void RegisterParticleAssetViews();
    void UnregisterParticleAssetViews();

} // namespace sky::particle::editor
