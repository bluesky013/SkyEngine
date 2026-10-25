#include "animation/editor/CharacterPreviewView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <core/type/TypeInfo.h>
#include <editor/core/EditorCore.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/ui/UiDraw.h>
#include <editor/widgets/ReflectedFormView.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <set>
#include <sstream>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>

namespace sky::animation::editor {
    using namespace sky::editor;
    using namespace sky::ui;
    namespace {
        struct BoneInspection {
            std::string name, parent, referenceTransform, modelMatrix;
        };
        struct PreviewParameter {
            Uuid        id;
            std::string name;
            bool        isBoolean    = false;
            bool        booleanValue = false;
            float       floatValue   = 0;
        };
        struct PreviewOverrides {
            std::vector<PreviewParameter> parameters;
        };
        void RegisterInspection()
        {
            RegisterCharacterPreviewSettings();
            auto *context = SerializationContext::Get();
            context->Register<BoneInspection>("SelectedBone")
                .Member<&BoneInspection::name>("name")
                .Property(CommonPropertyKey::READONLY, Any(true))
                .Member<&BoneInspection::parent>("parent")
                .Property(CommonPropertyKey::READONLY, Any(true))
                .Member<&BoneInspection::referenceTransform>("referenceTransform")
                .Property(CommonPropertyKey::READONLY, Any(true))
                .Member<&BoneInspection::modelMatrix>("modelMatrix")
                .Property(CommonPropertyKey::READONLY, Any(true));
            context->Register<PreviewParameter>("PreviewParameterOverride")
                .Member<&PreviewParameter::name>("name")
                .Property(CommonPropertyKey::READONLY, Any(true))
                .Member<&PreviewParameter::isBoolean>("isBoolean")
                .Property(CommonPropertyKey::READONLY, Any(true))
                .Member<&PreviewParameter::booleanValue>("booleanValue")
                .Member<&PreviewParameter::floatValue>("floatValue");
            context->Register<PreviewOverrides>("TemporaryPreviewOverrides").Member<&PreviewOverrides::parameters>("parameters");
        }
        void Line(UIPaintContext &context, Vector2 a, Vector2 b, float width, uint32_t color)
        {
            const float x = b.x - a.x, y = b.y - a.y, length = std::hypot(x, y);
            if (!std::isfinite(length) || length < .001f)
                return;
            const float dx = -y / length * width * .5f, dy = x / length * width * .5f;
            context.AddTriangle(a.x - dx, a.y - dy, a.x + dx, a.y + dy, b.x + dx, b.y + dy, color);
            context.AddTriangle(a.x - dx, a.y - dy, b.x + dx, b.y + dy, b.x - dx, b.y - dy, color);
        }
        class CharacterViewport final : public UIElement {
        public:
            CharacterViewport(CharacterPreview &preview, UITextSystem *text) : preview(preview), skin(GetDefaultUiTheme(), text)
            {
            }
            bool WantsPointerCapture() const override
            {
                return orbit || scrub;
            }
            void OnPointerCancel(uint32_t) override
            {
                orbit = scrub = false;
            }
            UIEventResult OnPointerEvent(const UIPointerEvent &event) override
            {
                const auto &m = skin.Theme().metrics;
                if (event.action == UIPointerAction::WHEEL) {
                    if (tree.Contains(event.x, event.y)) {
                        scroll = std::max(0.f, scroll - event.wheelDelta * m.rowHeight);
                    } else if (image.Contains(event.x, event.y)) {
                        preview.Camera().Dolly(event.wheelDelta * preview.Camera().orbitDistance * .1f);
                    }
                    MarkPaintDirty();
                    return UIEventResult::HANDLED;
                }
                if (event.action == UIPointerAction::DOWN) {
                    if (timeline.Contains(event.x, event.y) && !preview.IsGraph() && preview.CanPlay()) {
                        scrub = true;
                        preview.Control(AnimationControlOp::Pause);
                    } else if (tree.Contains(event.x, event.y)) {
                        const auto rows = VisibleBones();
                        const auto row  = static_cast<size_t>((event.y - tree.top + scroll) / m.rowHeight);
                        if (row < rows.size()) {
                            preview.selectedBone = rows[row].first;
                            if (event.x < tree.left + (rows[row].second + 1) * m.indentX) {
                                const auto id = rows[row].first;
                                if (collapsed.contains(id))
                                    collapsed.erase(id);
                                else
                                    collapsed.insert(id);
                            }
                        }
                    } else if (image.Contains(event.x, event.y)) {
                        orbit = true;
                    }
                    previous = {event.x, event.y};
                }
                if (scrub && (event.action == UIPointerAction::DOWN || event.action == UIPointerAction::MOVE)) {
                    const float fraction = std::clamp((event.x - timeline.left) / std::max(1.f, timeline.Width()), 0.f, 1.f);
                    preview.Control(AnimationControlOp::Seek, fraction * preview.Playback().duration);
                } else if (orbit && event.action == UIPointerAction::MOVE) {
                    preview.Camera().Look(event.x - previous.x, event.y - previous.y, .005f, true);
                    previous = {event.x, event.y};
                }
                if (event.action == UIPointerAction::UP)
                    orbit = scrub = false;
                MarkPaintDirty();
                return UIEventResult::HANDLED;
            }
            void OnPaint(UIPaintContext &context) override
            {
                const auto &theme = skin.Theme();
                const auto &m     = theme.metrics;
                skin.DrawPanel(context, GetBounds(), "Character preview");
                context.PushClip(GetBounds());
                const auto &output = preview.Output();
                if (output.texture)
                    context.AddTexturedQuad(image, {0, 0, 1, 1}, output.texture, theme.colors.white);
                else
                    skin.DrawField(context, image, false, false);
                context.PushClip(image);
                const auto project = [&](Vector3 point, Vector2 &screen) {
                    if (!preview.Camera().Project(point, {image.Width(), image.Height()}, screen))
                        return false;
                    screen.x += image.left;
                    screen.y += image.top;
                    return true;
                };
                const auto &rig    = preview.Rig();
                const auto &models = preview.BoneModels();
                const bool  ready  = output.state == AssetPreviewState::Ready && output.texture && preview.Diagnostic().empty() &&
                                   (!preview.Settings().mesh || !output.bounds.empty()) && (preview.Settings().referencePose || preview.CanPlay());
                if (!ready && skin.Text()) {
                    const UIRect notice{image.left + m.controlPad * 2, image.top + m.controlPad * 2, image.right - m.controlPad * 2,
                                        image.bottom - m.controlPad * 2};
                    skin.DrawField(context, notice, false, !preview.Diagnostic().empty());
                    uidraw::Text(
                        context, "Preview not ready", theme.fonts.title,
                        {notice.left + m.controlPad, notice.top + m.controlPad, notice.right - m.controlPad, notice.top + m.controlPad + m.rowHeight},
                        theme.colors.text, skin.Text());
                    std::string message = preview.Diagnostic().empty() ? "Loading preview resources..." : preview.Diagnostic();
                    message += "\n\nChoose Mesh, Skin and Skeleton in Preview settings.";
                    if (preview.IsGraph())
                        message += "\nSave the Graph to cook, then reload the successful product.";
                    const auto layout = UITextLayout::Build(message, theme.fonts.label, skin.Text()->GetAtlas(),
                                                            std::max(0.f, notice.Width() - 2 * m.controlPad), true);
                    context.PushClip(notice);
                    UITextLayout::Emit(context, layout, notice.left + m.controlPad, notice.top + 2 * m.rowHeight, theme.colors.textMuted,
                                       skin.Text()->GetAtlas());
                    context.PopClip();
                } else if (ready && !preview.Settings().mesh) {
                    uidraw::Text(context, "Skeleton only - choose a Mesh in Preview settings", theme.fonts.label,
                                 {image.left + m.controlPad, image.top, image.right - m.controlPad, image.top + m.rowHeight}, theme.colors.text,
                                 skin.Text());
                }
                if (ready && preview.Settings().showBones && rig.resource && models.size() == rig.resource->GetNumBones()) {
                    for (uint32_t i = 0; i < models.size(); ++i) {
                        Vector2     screen;
                        const auto &p = models[i].m[3];
                        if (!project({p.x, p.y, p.z}, screen))
                            continue;
                        const auto parent = rig.resource->GetBoneByIndex(static_cast<BoneIndex>(i))->parent;
                        Vector2    other;
                        if (parent < models.size()) {
                            const auto &q = models[parent].m[3];
                            if (project({q.x, q.y, q.z}, other))
                                Line(context, screen, other, m.selectionWidth, theme.colors.accent);
                        }
                        const float radius = m.selectionMarkerRadius * .5f;
                        skin.DrawField(context, {screen.x - radius, screen.y - radius, screen.x + radius, screen.y + radius},
                                       i == preview.selectedBone, false);
                    }
                }
                if (ready && preview.Settings().showBounds) {
                    for (const auto &bounds : output.bounds) {
                        std::array<Vector2, 8> points;
                        std::array<bool, 8>    visible;
                        for (uint32_t i = 0; i < 8; ++i)
                            visible[i] = project(
                                {i & 1 ? bounds.max.x : bounds.min.x, i & 2 ? bounds.max.y : bounds.min.y, i & 4 ? bounds.max.z : bounds.min.z},
                                points[i]);
                        for (uint32_t i = 0; i < 8; ++i)
                            for (uint32_t axis : {1u, 2u, 4u})
                                if (!(i & axis) && visible[i] && visible[i | axis])
                                    Line(context, points[i], points[i | axis], m.selectionWidth, theme.colors.gizmoHighlight);
                    }
                }
                context.PopClip();
                if (!preview.IsGraph())
                    skin.DrawSlider(context, timeline, preview.Playback().time, 0.f, std::max(.001f, preview.Playback().duration));
                const auto label = preview.IsGraph()
                                       ? "Drag to orbit | Wheel to zoom | Focus character"
                                       : std::to_string(preview.Playback().time) + " / " + std::to_string(preview.Playback().duration) + " s";
                uidraw::Text(context, label, theme.fonts.small, timeline, theme.colors.text, skin.Text(), uidraw::HAlign::Center);
                const auto rows = VisibleBones();
                scroll          = std::clamp(scroll, 0.f, std::max(0.f, float(rows.size()) * m.rowHeight - tree.Height()));
                context.PushClip(tree);
                float y = tree.top - scroll;
                if (rows.empty())
                    uidraw::Text(context, "No skeleton loaded", theme.fonts.label,
                                 {tree.left + m.controlPad, tree.top, tree.right, tree.top + m.rowHeight}, theme.colors.textMuted, skin.Text());
                for (const auto &[index, depth] : rows) {
                    const auto  *bone = rig.resource->GetBoneByIndex(static_cast<BoneIndex>(index));
                    const UIRect row{tree.left, y, tree.right, y + m.rowHeight};
                    skin.DrawRow(context, row, index == preview.selectedBone ? RowState::Selected : RowState::Normal);
                    const float x = tree.left + depth * m.indentX;
                    if (!bone->children.empty())
                        skin.DrawTriangle(context, x, y + m.rowHeight * .5f, !collapsed.contains(index), theme.colors.textMuted);
                    uidraw::Text(context, std::string(bone->name.GetStr()), theme.fonts.label, {x + m.indentX, y, tree.right, y + m.rowHeight},
                                 theme.colors.text, skin.Text());
                    y += m.rowHeight;
                }
                context.PopClip();
                context.PopClip();
            }

        protected:
            void ArrangeChildren(const UIRect &) override
            {
                const auto  content = skin.PanelContentRect(GetBounds());
                const auto &m       = skin.Theme().metrics;
                const float split   = content.top + std::max(0.f, content.Height() - m.rowHeight) * .65f;
                image               = {content.left, content.top, content.right, split};
                timeline            = {content.left, split, content.right, split + m.rowHeight};
                tree                = {content.left, timeline.bottom, content.right, content.bottom};
                preview.Resize(static_cast<uint32_t>(std::max(1.f, image.Width())), static_cast<uint32_t>(std::max(1.f, image.Height())));
            }

        private:
            std::vector<std::pair<uint32_t, uint32_t>> VisibleBones() const
            {
                std::vector<std::pair<uint32_t, uint32_t>> rows;
                const auto                                &rig = preview.Rig();
                if (!rig.resource)
                    return rows;
                std::vector<bool>     visible(rig.resource->GetNumBones(), false);
                std::vector<uint32_t> depth(visible.size(), 0);
                for (uint32_t i = 0; i < visible.size(); ++i) {
                    const auto parent = rig.resource->GetBoneByIndex(static_cast<BoneIndex>(i))->parent;
                    visible[i]        = parent == INVALID_BONE_ID || (parent < i && visible[parent] && !collapsed.contains(parent));
                    depth[i]          = parent < i ? depth[parent] + 1 : 0;
                    if (visible[i])
                        rows.emplace_back(i, depth[i]);
                }
                return rows;
            }
            CharacterPreview  &preview;
            UiSkin             skin;
            UIRect             image, timeline, tree;
            Vector2            previous;
            bool               orbit = false, scrub = false;
            float              scroll = 0;
            std::set<uint32_t> collapsed;
        };
    } // namespace

    void RegisterCharacterPreviewViewTypes()
    {
        RegisterInspection();
    }

    struct CharacterPreviewPanel::Impl {
        CharacterPreview        &preview;
        CharacterPreviewSettings settings;
        BoneInspection           bone;
        PreviewOverrides         overrides;
        CommandService           commands;
        ReflectedFormView       *form            = nullptr;
        bool                     pending         = false;
        uint64_t                 commandRevision = 0;
        explicit Impl(CharacterPreview &preview) : preview(preview), settings(preview.Settings())
        {
        }
        struct State {
            CharacterPreviewSettings settings;
            PreviewOverrides         overrides;
        };
        State Read() const
        {
            return {settings, overrides};
        }
        void Write(const State &state)
        {
            settings  = state.settings;
            overrides = state.overrides;
        }
        class Edit final : public UndoCommand {
        public:
            Edit(Impl &owner, State before, State after)
                : UndoCommand("Edit temporary preview settings"), owner(owner), before(std::move(before)), after(std::move(after))
            {
            }
            void Do() override
            {
                owner.Write(after);
                owner.pending = true;
            }
            void Undo() override
            {
                owner.Write(before);
                owner.pending = true;
            }

        private:
            Impl &owner;
            State before, after;
        };
    };
    CharacterPreviewPanel::CharacterPreviewPanel(CharacterPreview &preview, UITextSystem *text) : impl(std::make_unique<Impl>(preview))
    {
        impl->form = static_cast<ReflectedFormView *>(AddChild(std::make_unique<ReflectedFormView>(
            text, "Preview settings and selected bone",
            ReflectedFormContext{impl->commands, EditorCore::GetPropertyEditors(), EditorCore::GetPropertyChanges(), GetDefaultUiTheme()})));
        impl->form->BindGroups(std::array{PropertyObject{&impl->settings, GetTypeNode(TypeInfo<CharacterPreviewSettings>::RegisteredId())},
                                          PropertyObject{&impl->bone, GetTypeNode(TypeInfo<BoneInspection>::RegisteredId())},
                                          PropertyObject{&impl->overrides, GetTypeNode(TypeInfo<PreviewOverrides>::RegisteredId())}});
        impl->form->SetOnEdited([this] { impl->pending = true; });
        impl->form->Form().SetEditCommandFactory([this](const PropertyDescriptor &field, Any value) -> UndoCommandPtr {
            const auto before = impl->Read();
            if (!field.SetValue(value))
                return {};
            if (impl->settings.mesh != before.settings.mesh && impl->settings.mesh && !impl->settings.skin) {
                auto defaults = impl->settings;
                if (ApplyImportedCharacterDefaults(defaults) && (!impl->settings.skeleton || impl->settings.skeleton == defaults.skeleton))
                    impl->settings = defaults;
            }
            auto after = impl->Read();
            impl->Write(before);
            const auto &a = before.overrides.parameters;
            const auto &b = after.overrides.parameters;
            if (a.size() != b.size() || !std::equal(a.begin(), a.end(), b.begin(), [](const auto &left, const auto &right) {
                    return left.id == right.id && left.name == right.name && left.isBoolean == right.isBoolean;
                })) {
                impl->preview.ReportError("Preview overrides cannot change Graph parameter definitions");
                return {};
            }
            return std::make_unique<Impl::Edit>(*impl, before, std::move(after));
        });
    }
    CharacterPreviewPanel::~CharacterPreviewPanel()
    {
        Cancel();
        ClearChildren();
    }
    void CharacterPreviewPanel::Cancel()
    {
        impl->form->CancelInteractions();
    }
    bool CharacterPreviewPanel::Editing() const
    {
        return impl->form->Form().IsEditing() || impl->form->HasActiveInteraction();
    }
    CommandService &CharacterPreviewPanel::History()
    {
        return impl->commands;
    }
    void CharacterPreviewPanel::ApplyDefaults()
    {
        Cancel();
        auto       next   = impl->settings;
        const auto status = ApplyImportedCharacterDefaults(next);
        if (!status) {
            impl->preview.ReportError(status.message);
            return;
        }
        auto after     = impl->Read();
        after.settings = next;
        impl->commands.Execute(std::make_unique<Impl::Edit>(*impl, impl->Read(), std::move(after)));
    }
    void CharacterPreviewPanel::Sync()
    {
        if (impl->form->HasActiveInteraction() || Editing())
            return;
        if (impl->pending || impl->commands.GetRevision() != impl->commandRevision) {
            impl->preview.Configure(impl->settings);
            for (const auto &p : impl->overrides.parameters)
                impl->preview.SetParameter(p.id, p.isBoolean ? AnimationParam(p.booleanValue) : AnimationParam(p.floatValue));
            impl->pending         = false;
            impl->commandRevision = impl->commands.GetRevision();
        }
        impl->settings    = impl->preview.Settings();
        impl->bone        = {};
        const auto &rig   = impl->preview.Rig();
        const auto  index = impl->preview.selectedBone;
        if (rig.resource && index < rig.resource->GetNumBones()) {
            const auto *bone  = rig.resource->GetBoneByIndex(static_cast<BoneIndex>(index));
            impl->bone.name   = bone->name.GetStr();
            impl->bone.parent = bone->parent == INVALID_BONE_ID ? "Root" : std::string(rig.resource->GetBoneByIndex(bone->parent)->name.GetStr());
            const auto        &transform = rig.resource->GetRefPos()->transforms[index];
            std::ostringstream text;
            text << "T " << transform.translation.x << ", " << transform.translation.y << ", " << transform.translation.z << " R "
                 << transform.rotation.x << ", " << transform.rotation.y << ", " << transform.rotation.z << ", " << transform.rotation.w << " S "
                 << transform.scale.x << ", " << transform.scale.y << ", " << transform.scale.z;
            impl->bone.referenceTransform = text.str();
            if (index < impl->preview.BoneModels().size()) {
                std::ostringstream matrix;
                for (float value : impl->preview.BoneModels()[index].v)
                    matrix << value << ' ';
                impl->bone.modelMatrix = matrix.str();
            }
        }
        impl->overrides.parameters.clear();
        for (const auto &p : impl->preview.Playback().parameters) {
            const bool boolean = std::holds_alternative<bool>(p.value);
            impl->overrides.parameters.push_back(
                {p.id, p.name, boolean, boolean ? std::get<bool>(p.value) : false, boolean ? 0.f : std::get<float>(p.value)});
        }
        impl->form->Refresh();
        impl->form->SynchronizeLayout();
        MarkPaintDirty();
    }
    bool CharacterPreviewPanel::WantsPointerCapture() const
    {
        return impl->form->WantsPointerCapture();
    }
    UIEventResult CharacterPreviewPanel::OnPointerEvent(const UIPointerEvent &event)
    {
        return impl->form->OnPointerEvent(event);
    }
    UIEventResult CharacterPreviewPanel::OnKeyEvent(const UIKeyEvent &event)
    {
        return impl->form->OnKeyEvent(event);
    }
    UIEventResult CharacterPreviewPanel::OnTextInput(const UITextInputEvent &event)
    {
        return impl->form->OnTextInput(event);
    }
    void CharacterPreviewPanel::OnPointerCancel(uint32_t pointer)
    {
        impl->form->OnPointerCancel(pointer);
    }
    UISize CharacterPreviewPanel::OnMeasure(const UIConstraints &constraints)
    {
        return constraints.Constrain({GetDefaultUiTheme().metrics.fieldWidth * 2, GetDefaultUiTheme().metrics.rowHeight * 20});
    }
    void CharacterPreviewPanel::ArrangeChildren(const UIRect &content)
    {
        impl->form->Arrange(content);
    }
    std::unique_ptr<UIElement> CreateCharacterViewport(CharacterPreview &preview, UITextSystem *text)
    {
        return std::make_unique<CharacterViewport>(preview, text);
    }
    CharacterPreviewActions::CharacterPreviewActions(std::shared_ptr<CharacterPreview> preview, std::function<void()> defaults, std::string scope)
        : scope(std::move(scope))
    {
        const std::weak_ptr<void> alive = lifetime;
        const auto add = [&](std::string suffix, std::string label, std::string icon, std::function<void()> action, bool playback = false) {
            const auto   id = "animation.preview." + suffix;
            EditorAction entry;
            entry.moduleLease = preview->CodeLease();
            entry.id          = id;
            entry.label       = std::move(label);
            entry.icon        = std::move(icon);
            entry.group       = EditorGroup::Play;
            entry.menu        = EditorMenu::View;
            entry.submenu     = "Preview";
            entry.scope       = this->scope;
            entry.enabled     = [alive, preview, playback] { return !alive.expired() && (!playback || preview->CanPlay()); };
            entry.invoke      = [alive, preview, action = std::move(action)] {
                if (!alive.expired())
                    action();
            };
            EditorActionRegistry::Get()->Add(std::move(entry));
            ids.push_back(id);
        };
        add("play", "Play preview", "play", [preview] { preview->Control(AnimationControlOp::Play); }, true);
        add("pause", "Pause preview", "pause", [preview] { preview->Control(AnimationControlOp::Pause); }, true);
        add("stop", "Stop preview", "stop", [preview] { preview->Control(AnimationControlOp::Stop); }, true);
        add("reload", "Reload successful product", "open", [preview] { preview->Control(AnimationControlOp::Reload); });
        add("defaults", "Apply imported binding defaults", "coordinates", std::move(defaults));
        add("focus", "Focus character", "focus", [preview] {
            const auto status = preview->FocusCharacter();
            preview->ReportError(status.message);
        });
    }
    CharacterPreviewActions::~CharacterPreviewActions()
    {
        lifetime.reset();
        for (const auto &id : ids)
            EditorActionRegistry::Get()->Remove(scope, id);
    }
    namespace {
        class CharacterDocument final : public IAssetViewDocument {
        public:
            explicit CharacterDocument(std::shared_ptr<CharacterPreview> preview) : preview(std::move(preview))
            {
            }
            ~CharacterDocument() override
            {
                actions.reset();
                preview->Close();
            }
            void SetActionScope(std::string value) override
            {
                scope = std::move(value);
            }
            std::unique_ptr<UIElement> CreateContent(UITextSystem *text) override
            {
                auto result = std::make_unique<CharacterPreviewPanel>(*preview, text);
                panel       = result.get();
                actions     = std::make_unique<CharacterPreviewActions>(preview, [this] { panel->ApplyDefaults(); }, scope);
                return result;
            }
            std::unique_ptr<UIElement> CreatePreview(UITextSystem *text) override
            {
                return CreateCharacterViewport(*preview, text);
            }
            CommandService &History() override
            {
                return panel->History();
            }
            bool IsDirty() const override
            {
                return false;
            }
            bool IsEditing() const override
            {
                return panel && panel->Editing();
            }
            void CancelInteractions() override
            {
                if (panel)
                    panel->Cancel();
            }
            void RequestSave() override
            {
            }
            bool IsSavePending() const override
            {
                return false;
            }
            std::string Status() const override
            {
                return preview->Diagnostic().empty() ? "Preview settings are temporary; source is read-only" : preview->Diagnostic();
            }
            void Tick(float delta) override
            {
                if (panel)
                    panel->Sync();
                preview->Tick(delta);
            }

        private:
            std::shared_ptr<CharacterPreview>        preview;
            std::string                              scope = "assetViewer";
            CharacterPreviewPanel                   *panel = nullptr;
            std::unique_ptr<CharacterPreviewActions> actions;
        };
    } // namespace
    std::unique_ptr<IAssetViewDocument> CreateCharacterEditor(std::shared_ptr<CharacterPreview> preview)
    {
        return preview ? std::make_unique<CharacterDocument>(std::move(preview)) : nullptr;
    }
} // namespace sky::animation::editor
