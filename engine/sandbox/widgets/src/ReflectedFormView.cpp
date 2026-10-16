//
// Created on 2026/10/04.
//

#include <editor/ui/UiDraw.h>
#include <editor/widgets/AssetReflectedWidget.h>
#include <editor/widgets/ColorReflectedWidget.h>
#include <editor/widgets/EnumReflectedWidget.h>
#include <editor/widgets/GenericReflectedWidget.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/RotationReflectedWidget.h>

#include <editor/core/EditorCore.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/core/property/PropertyValidation.h>

#include <core/type/TypeInfo.h>
#include <core/util/Uuid.h>
#include <framework/serialization/SerializationContext.h>

#include <ui/text/UITextSystem.h>

#include <memory>
#include <unordered_set>
#include <utility>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

        constexpr float    kValueBoxW    = 60.0f;
        constexpr uint32_t kKeyBackspace = 0x08;
        constexpr uint32_t kKeyReturn    = 0x0D;
        constexpr uint32_t kKeyEscape    = 0x1B;
        constexpr uint32_t kKeyLeft      = 0x25;
        constexpr uint32_t kKeyRight     = 0x27;
        constexpr uint32_t kKeyHome      = 0x24;
        constexpr uint32_t kKeyEnd       = 0x23;
        constexpr uint32_t kKeyDelete    = 0x2E;
        constexpr uint32_t kKeyZ         = 0x5A;
        constexpr uint32_t kKeyY         = 0x59;

        class WidgetDispatchScope {
        public:
            WidgetDispatchScope(std::shared_ptr<ReflectedWidget> &current, std::shared_ptr<ReflectedWidget> widget)
                : current(current), previous(std::exchange(current, std::move(widget)))
            {
            }
            ~WidgetDispatchScope()
            {
                current = std::move(previous);
            }

        private:
            std::shared_ptr<ReflectedWidget> &current;
            std::shared_ptr<ReflectedWidget>  previous;
        };

        int64_t EnumRaw(const Any &value)
        {
            int64_t           out  = 0;
            const TypeInfoRT *info = value.Info();
            if (info == nullptr || info->staticInfo == nullptr || value.Data() == nullptr) {
                return 0;
            }
            const size_t size = std::min<size_t>(info->staticInfo->size, sizeof(out));
            std::memcpy(&out, value.Data(), size);
            return out;
        }

        std::string FormatValueText(const PropertyDescriptor &descriptor, const EditorControl &control)
        {
            const Any         value = descriptor.GetValue();
            const TypeInfoRT *info  = value.Info();
            if (info == nullptr || info->staticInfo == nullptr || value.Data() == nullptr) {
                return {};
            }
            if (info->staticInfo->isEnum) {
                const int64_t raw = EnumRaw(value);
                if (const TypeNode *node = GetTypeNode(info)) {
                    const auto iter = node->enums.find(static_cast<uint64_t>(raw));
                    if (iter != node->enums.end()) {
                        return std::string(iter->second);
                    }
                }
                return std::to_string(raw);
            }
            if (info->registeredId == TypeInfo<bool>::RegisteredId()) {
                return (value.GetAsConst<bool>() != nullptr && *value.GetAsConst<bool>()) ? "true" : "false";
            }
            if (info->registeredId == TypeInfo<std::string>::RegisteredId()) {
                const std::string *s = value.GetAsConst<std::string>();
                return s != nullptr ? *s : std::string();
            }

            char      buffer[64] = {0};
            const int decimals   = DecimalsForControl(control);
            if (const float *v = value.GetAsConst<float>()) {
                std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(*v));
            } else if (const double *v = value.GetAsConst<double>()) {
                std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, *v);
            } else if (const int32_t *v = value.GetAsConst<int32_t>()) {
                std::snprintf(buffer, sizeof(buffer), "%d", *v);
            } else if (const uint32_t *v = value.GetAsConst<uint32_t>()) {
                std::snprintf(buffer, sizeof(buffer), "%u", *v);
            } else if (const int64_t *v = value.GetAsConst<int64_t>()) {
                std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(*v));
            } else if (const uint64_t *v = value.GetAsConst<uint64_t>()) {
                std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(*v));
            } else {
                return info->name.empty() ? std::string("<value>") : std::string(info->name);
            }
            return buffer;
        }

    } // namespace

    ReflectedFormView::ReflectedFormView(sky::ui::UITextSystem *text, std::string inTitle)
        : ReflectedFormView(
              text,
              std::move(inTitle),
              {EditorCore::GetCommandService(), EditorCore::GetPropertyEditors(), EditorCore::GetPropertyChanges(), GetDefaultUiTheme()})
    {
    }

    ReflectedFormView::ReflectedFormView(sky::ui::UITextSystem *text, std::string inTitle, const ReflectedFormContext &context)
        : textSystem(text)
        , title(std::move(inTitle))
        , commands(&context.commands)
        , registry(&context.propertyEditors)
        , propertyChanges(&context.propertyChanges)
        , skin(context.theme, text)
    {
        SetFocusable(true);
        EditorCore::RegisterDefaultPropertyEditors(*registry);
        widgets.Register(PropertyEditorKind::Color, std::make_shared<ColorReflectedWidget>());
        widgets.Register(PropertyEditorKind::Rotation, std::make_shared<RotationReflectedWidget>());
        widgets.Register(PropertyEditorKind::Asset, std::make_shared<AssetReflectedWidget>());
        widgets.Register(PropertyEditorKind::Enum, std::make_shared<EnumReflectedWidget>());
        auto generic = std::make_shared<GenericReflectedWidget>();
        for (PropertyEditorKind kind : {PropertyEditorKind::Bool, PropertyEditorKind::Integer, PropertyEditorKind::Float, PropertyEditorKind::String,
                                        PropertyEditorKind::Vector}) {
            widgets.Register(kind, generic);
        }
        lastRevision                         = propertyChanges->GetRevision();
        const std::weak_ptr<void> lifetime   = subscriptionLifetime;
        const auto                invalidate = [this, lifetime]() {
            if (const auto alive = lifetime.lock()) {
                InvalidateExternalState();
            }
        };
        propertyChangeId = propertyChanges->Subscribe(invalidate);
        commandChangeId  = commands->AddChangeCallback(invalidate);
        beforeHistoryId  = commands->AddBeforeHistoryCallback([this, lifetime]() {
            if (const auto alive = lifetime.lock()) {
                CancelInteractions();
            }
        });
        widgets.SetOnChanged([this, lifetime]() {
            if (const auto alive = lifetime.lock()) {
                MarkLayoutDirty();
            }
        });
    }

    ReflectedFormView::~ReflectedFormView()
    {
        subscriptionLifetime.reset();
        propertyChanges->Unsubscribe(propertyChangeId);
        commands->RemoveChangeCallback(commandChangeId);
        commands->RemoveBeforeHistoryCallback(beforeHistoryId);
        ResetInteractions(ReflectedInteractionReset::Abandon);
    }

    CommandService &ReflectedFormView::Commands()
    {
        return *commands;
    }
    void ReflectedFormView::MarkDirty()
    {
        MarkPaintDirty();
    }
    void ReflectedFormView::RefreshForm()
    {
        Refresh();
    }

    void ReflectedFormView::Bind(const PropertyObject &object, const PropertyObject *baseline)
    {
        ResetInteractions(ReflectedInteractionReset::Abandon);
        form.Build(object, *registry, baseline);
        lastRevision           = propertyChanges->GetRevision();
        pendingExternalRefresh = false;
        MarkLayoutDirty();
    }

    void ReflectedFormView::Unbind()
    {
        Bind(PropertyObject{});
    }

    void ReflectedFormView::CancelInteractions()
    {
        ResetInteractions();
    }

    void ReflectedFormView::Refresh()
    {
        std::unordered_set<std::string> expandedPaths;
        const auto                      saveExpansion = [&expandedPaths](auto &self, const std::vector<PropertyField> &fields) -> void {
            for (const auto &field : fields) {
                if (field.expanded) {
                    expandedPaths.insert(field.path);
                }
                self(self, field.children);
            }
        };
        for (const auto &section : form.GetSections()) {
            saveExpansion(saveExpansion, section.fields);
        }
        ResetInteractions();
        form.Rebuild();
        const auto restoreExpansion = [&expandedPaths](auto &self, std::vector<PropertyField> &fields) -> void {
            for (auto &field : fields) {
                field.expanded = expandedPaths.count(field.path) != 0;
                self(self, field.children);
            }
        };
        for (auto &section : form.GetSections()) {
            restoreExpansion(restoreExpansion, section.fields);
        }
        lastRevision           = propertyChanges->GetRevision();
        pendingExternalRefresh = false;
        MarkLayoutDirty();
    }

    void ReflectedFormView::SynchronizeLayout()
    {
        PollExternalChanges();
    }

    void ReflectedFormView::InvalidateExternalState()
    {
        pendingExternalRefresh = true;
        MarkLayoutDirty();
    }

    void ReflectedFormView::ResetInteractions(ReflectedInteractionReset mode)
    {
        if (mode == ReflectedInteractionReset::Abandon) {
            bindingLifetime.reset();
        }
        auto active   = std::move(activeWidget);
        hoverField    = nullptr;
        editField     = nullptr;
        editCommit    = nullptr;
        editComponent = -1;
        editInvalid   = false;
        editText.clear();
        editCaret     = 0;
        dragField     = nullptr;
        dragComponent = -1;
        scrollDrag    = false;
        hoverScroll   = false;
        rows.clear();
        widgets.ResetInteractions(*this, mode, std::move(active));
        bindingLifetime = std::make_shared<int>(0);
        MarkArrangeDirty();
    }

    bool ReflectedFormView::IsInteracting() const
    {
        return editField != nullptr || dragField != nullptr || scrollDrag || (activeWidget && activeWidget->IsInteracting()) ||
               widgets.IsInteracting();
    }

    std::shared_ptr<ReflectedWidget> ReflectedFormView::ActivePopup() const
    {
        return activeWidget && activeWidget->HasPopup() ? activeWidget : widgets.FindPopupShared();
    }

    sky::ui::UIRect ReflectedFormView::ContentRect() const
    {
        const sky::ui::UIRect bounds  = GetBounds();
        const float           headerH = headerVisible ? skin.Theme().metrics.headerHeight : 0.0f;
        return {bounds.left, bounds.top + headerH, bounds.right, bounds.bottom};
    }

    void ReflectedFormView::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (IsLayoutVolatile()) {
            // Compatibility for specialized panels whose ticks or widget row
            // counts still depend on the old paint-driven update path.
            SynchronizeLayout();
            OnViewTick();
            BuildRows();
        }

        const UiTheme        &th     = skin.Theme();
        const sky::ui::UIRect bounds = GetBounds();
        uc::RoundedField(context, bounds, th.colors.panel, th.colors.borderSoft, th.metrics.panelRadius);
        if (headerVisible) {
            DrawHeader(context, bounds);
        }
        const sky::ui::UIRect content = ContentRect();
        context.PushClip(content);
        float y = content.top + 4.0f - scroll;
        for (auto &section : form.GetSections()) {
            DrawSection(context, section, y);
        }
        context.PopClip();

        const uint32_t thumbColor = (scrollDrag || hoverScroll) ? uc::color::Text : th.colors.scrollBar;
        uc::ScrollBar(context, content, contentHeight, scroll, thumbColor);
        if (auto popup = ActivePopup(); popup != nullptr) {
            popup->PaintPopup(*this, context);
        }
    }

    float ReflectedFormView::MeasureContentHeight() const
    {
        const UiMetrics &m             = skin.Theme().metrics;
        float            height        = 12.0f;
        const auto       measureFields = [this, &m](auto &self, const std::vector<PropertyField> &fields) -> float {
            float result = 0.0f;
            for (const auto &field : fields) {
                result += m.rowHeight * static_cast<float>(WidgetRowCount(field));
                if ((field.isStruct || field.isSequence) && field.expanded) {
                    result += self(self, field.children);
                }
            }
            return result;
        };
        for (const auto &section : form.GetSections()) {
            height += m.sectionHeight + measureFields(measureFields, section.fields) + 8.0f;
        }
        return height;
    }

    float ReflectedFormView::GetPreferredHeight() const
    {
        const float headerH = headerVisible ? skin.Theme().metrics.headerHeight : 0.0f;
        return headerH + MeasureContentHeight() + 12.0f;
    }

    sky::ui::UISize ReflectedFormView::OnMeasure(const sky::ui::UIConstraints &constraints)
    {
        (void)constraints;
        return {0.0f, GetPreferredHeight()};
    }

    void ReflectedFormView::ArrangeChildren(const sky::ui::UIRect &content)
    {
        contentHeight = MeasureContentHeight();
        scroll        = std::clamp(scroll, 0.0f, std::max(0.0f, contentHeight - ContentRect().Height()));
        BuildRows();
        ComputeHeaderRects();
        UIElement::ArrangeChildren(content);
    }

    bool ReflectedFormView::WantsPointerCapture() const
    {
        return ActivePopup() != nullptr;
    }

    sky::ui::UIEventResult ReflectedFormView::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        PollExternalChanges();
        BuildRows();
        if (auto popup = ActivePopup(); popup != nullptr) {
            const bool handled = popup->OnPopupPointer(*this, event);
            if (!popup->IsInteracting() && activeWidget == popup) {
                activeWidget.reset();
            }
            return handled ? sky::ui::UIEventResult::HANDLED : sky::ui::UIEventResult::UNHANDLED;
        }

        // Generic scrollbar: hover + drag (shared by every reflected form).
        {
            const sky::ui::UIRect track  = ContentRect();
            const bool            hasBar = contentHeight > track.Height() && track.Height() > 0.0f;
            const bool  onBar = hasBar && event.x >= track.right - 10.0f && event.x <= track.right && event.y >= track.top && event.y <= track.bottom;
            const float ratio = hasBar ? std::clamp(track.Height() / contentHeight, 0.08f, 1.0f) : 1.0f;
            const float thumbH    = track.Height() * ratio;
            const float maxScroll = std::max(0.0f, contentHeight - track.Height());

            if (event.action == sky::ui::UIPointerAction::MOVE) {
                if (scrollDrag) {
                    const float denom = std::max(1.0f, track.Height() - thumbH);
                    const float t     = std::clamp((event.y - track.top - scrollDragOffset) / denom, 0.0f, 1.0f);
                    scroll            = t * maxScroll;
                    MarkArrangeDirty();
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (onBar != hoverScroll) {
                    hoverScroll = onBar;
                    MarkPaintDirty();
                }
                if (onBar) {
                    return sky::ui::UIEventResult::HANDLED;
                }
            } else if (event.action == sky::ui::UIPointerAction::DOWN && onBar) {
                const float t0  = maxScroll > 0.0f ? std::clamp(scroll / maxScroll, 0.0f, 1.0f) : 0.0f;
                const float top = track.top + (track.Height() - thumbH) * t0;
                if (event.y >= top && event.y <= top + thumbH) {
                    scrollDragOffset = event.y - top;
                } else {
                    scrollDragOffset  = thumbH * 0.5f; // clicking the track jumps the thumb to the cursor
                    const float denom = std::max(1.0f, track.Height() - thumbH);
                    scroll            = std::clamp((event.y - track.top - scrollDragOffset) / denom, 0.0f, 1.0f) * maxScroll;
                }
                scrollDrag = true;
                MarkArrangeDirty();
                return sky::ui::UIEventResult::HANDLED;
            } else if (event.action == sky::ui::UIPointerAction::UP && scrollDrag) {
                scrollDrag = false;
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
        }

        if (event.action == sky::ui::UIPointerAction::WHEEL) {
            const UiMetrics &m         = skin.Theme().metrics;
            const float      headerH   = headerVisible ? m.headerHeight : 0.0f;
            const float      viewH     = std::max(0.0f, GetBounds().bottom - GetBounds().top - headerH);
            const float      maxScroll = std::max(0.0f, contentHeight - viewH);
            if (maxScroll <= 0.0f) {
                return sky::ui::UIEventResult::UNHANDLED; // nothing to scroll: let an ancestor handle it
            }
            scroll = std::clamp(scroll - event.wheelDelta * 0.6f, 0.0f, maxScroll);
            MarkArrangeDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::MOVE) {
            if (activeWidget != nullptr) {
                auto active = activeWidget;
                active->OnMove(*this, event);
                return sky::ui::UIEventResult::HANDLED;
            }
            ComputeHeaderRects();
            if (HandleExtraHeaderPointer(event) != sky::ui::UIEventResult::UNHANDLED) {
                return sky::ui::UIEventResult::HANDLED;
            }
            const Row     *row   = RowAt(event.x, event.y);
            PropertyField *hover = row != nullptr ? row->field : nullptr;
            if (hover != hoverField) {
                hoverField = hover;
                MarkPaintDirty();
            }
            return hoverField != nullptr ? sky::ui::UIEventResult::HANDLED : sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::UP) {
            if (activeWidget != nullptr) {
                auto active = activeWidget;
                active->OnUp(*this, event);
                activeWidget = nullptr;
                return sky::ui::UIEventResult::HANDLED;
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        ComputeHeaderRects();
        if (const sky::ui::UIEventResult extra = HandleExtraHeaderPointer(event); extra != sky::ui::UIEventResult::UNHANDLED) {
            return extra;
        }

        const Row *row = RowAt(event.x, event.y);
        if (row == nullptr) {
            if (editField != nullptr) {
                CommitEdit();
            }
            return editField != nullptr ? sky::ui::UIEventResult::HANDLED : sky::ui::UIEventResult::UNHANDLED;
        }
        if (editField != nullptr && editField != row->field) {
            CommitEdit();
            if (editField != nullptr) {
                return sky::ui::UIEventResult::HANDLED; // invalid input keeps its field active
            }
            // The commit may rebuild every PropertyField and Row.
            BuildRows();
            row = RowAt(event.x, event.y);
            if (row == nullptr) {
                return sky::ui::UIEventResult::HANDLED;
            }
        }
        // Widgets own only the control rect; clicks elsewhere on the row (label,
        // revert icon) fall through to the view's row interaction. Widget OnDown
        // implementations assume the pointer is inside the control rect.
        if (row->controlRect.Contains(event.x, event.y)) {
            if (auto widget = widgets.FindShared(row->field->kind); widget != nullptr) {
                WidgetDispatchScope dispatch(dispatchWidget, widget);
                if (widget->OnDown(*this, *row->field, event, row->controlRect)) {
                    activeWidget = widget;
                    return sky::ui::UIEventResult::HANDLED;
                }
            }
        }
        BeginInteraction(*row, event.x, event.y);
        return sky::ui::UIEventResult::HANDLED;
    }

    void ReflectedFormView::OnPointerCancel(uint32_t pointerId)
    {
        (void)pointerId;
        ResetInteractions();
    }

    sky::ui::UIEventResult ReflectedFormView::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        if (event.action == sky::ui::UIKeyAction::DOWN && (event.modifiers & kModCtrl) != 0 && (event.keyCode == kKeyZ || event.keyCode == kKeyY)) {
            const std::weak_ptr<void> lifetime  = subscriptionLifetime;
            const bool                redo      = event.keyCode == kKeyY || (event.modifiers & kModShift) != 0;
            const bool                traversed = redo ? commands->Redo() : commands->Undo();
            if (traversed && !lifetime.expired()) {
                Refresh();
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        if (editField != nullptr) {
            if (event.action == sky::ui::UIKeyAction::UP) {
                return sky::ui::UIEventResult::HANDLED;
            }
            switch (event.keyCode) {
            case kKeyBackspace:
                if (editCaret > 0) {
                    editText.erase(editCaret - 1, 1);
                    --editCaret;
                }
                editInvalid = false;
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            case kKeyDelete:
                if (editCaret < editText.size()) {
                    editText.erase(editCaret, 1);
                }
                editInvalid = false;
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            case kKeyLeft:
                editCaret = editCaret > 0 ? editCaret - 1 : 0;
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            case kKeyRight:
                editCaret = std::min(editCaret + 1, editText.size());
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            case kKeyHome:
                editCaret = 0;
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            case kKeyEnd:
                editCaret = editText.size();
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            case kKeyReturn: CommitEdit(); return sky::ui::UIEventResult::HANDLED;
            case kKeyEscape: CancelEdit(); return sky::ui::UIEventResult::HANDLED;
            default: return sky::ui::UIEventResult::HANDLED;
            }
        }

        if (event.keyCode == kKeyEscape) {
            if (auto popup = ActivePopup(); popup != nullptr) {
                popup->OnEscape(*this);
                if (!popup->IsInteracting() && activeWidget == popup) {
                    activeWidget.reset();
                }
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    sky::ui::UIEventResult ReflectedFormView::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        if (editField == nullptr || event.text.empty()) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        for (char c : event.text) {
            if (IsInputCharAllowed(editKind, c)) {
                editText.insert(editCaret, 1, c);
                ++editCaret;
            }
        }
        editInvalid = false;
        MarkPaintDirty();
        return sky::ui::UIEventResult::HANDLED;
    }

    void ReflectedFormView::OnPointerLeave(const sky::ui::UIPointerEvent &event)
    {
        (void)event;
        if (hoverField != nullptr) {
            hoverField = nullptr;
            MarkPaintDirty();
        }
    }

    void ReflectedFormView::PollExternalChanges()
    {
        const uint64_t revision = propertyChanges->GetRevision();
        if (revision != lastRevision) {
            lastRevision           = revision;
            pendingExternalRefresh = true;
        }
        // A revision observed while editing remains pending until the next safe
        // poll, even if the notifier has not advanced again in the meantime.
        if (pendingExternalRefresh && !IsInteracting()) {
            Refresh();
        }
    }

    void ReflectedFormView::BuildRows()
    {
        contentHeight = MeasureContentHeight();
        rows.clear();
        const UiMetrics      &m      = skin.Theme().metrics;
        const sky::ui::UIRect bounds = GetBounds();
        layoutLeft                   = bounds.left + m.padX;
        layoutRight                  = bounds.right - m.padX - m.scrollBarWidth;
        labelWidth                   = std::min(240.0f, (layoutRight - layoutLeft) * 0.42f);

        const float headerH = headerVisible ? m.headerHeight : 0.0f;
        float       y       = bounds.top + headerH + 4.0f - scroll;
        bool        alt     = false;
        for (auto &section : form.GetSections()) {
            y += m.sectionHeight;
            for (auto &field : section.fields) {
                alt = false;
                LayoutField(field, 0, y, alt);
            }
            y += 8.0f;
        }
    }

    void ReflectedFormView::LayoutField(PropertyField &field, int depth, float &y, bool &alt)
    {
        const UiMetrics &m      = skin.Theme().metrics;
        const float      indent = m.indentX * static_cast<float>(depth);
        const float      h      = m.rowHeight * static_cast<float>(std::max(1, WidgetRowCount(field)));
        Row              row;
        row.field       = &field;
        row.depth       = depth;
        row.alt         = alt;
        row.rowRect     = sky::ui::UIRect{layoutLeft, y, layoutRight, y + h};
        row.labelRect   = sky::ui::UIRect{layoutLeft + indent + 4.0f, y, layoutLeft + indent + labelWidth, y + h};
        row.revertRect  = sky::ui::UIRect{layoutRight - 16.0f, y + 1.0f, layoutRight, y + h - 1.0f};
        row.controlRect = sky::ui::UIRect{layoutLeft + indent + labelWidth + m.controlPad, y + 1.0f, layoutRight - 20.0f, y + h - 1.0f};
        rows.push_back(row);
        alt = !alt;
        y += h;

        if ((field.isStruct || field.isSequence) && field.expanded) {
            for (auto &child : field.children) {
                LayoutField(child, depth + 1, y, alt);
            }
        }
    }

    const ReflectedFormView::Row *ReflectedFormView::RowAt(float x, float y) const
    {
        for (const auto &row : rows) {
            if (row.rowRect.Contains(x, y)) {
                return &row;
            }
        }
        return nullptr;
    }

    const ReflectedFormView::Row *ReflectedFormView::RowOf(const PropertyField *field) const
    {
        for (const auto &row : rows) {
            if (row.field == field) {
                return &row;
            }
        }
        return nullptr;
    }

    sky::ui::UIRect ReflectedFormView::SliderRect(const sky::ui::UIRect &control) const
    {
        return sky::ui::UIRect{control.left, control.top, std::max(control.left, control.right - kValueBoxW - 6.0f), control.bottom};
    }

    sky::ui::UIRect ReflectedFormView::VectorCell(const sky::ui::UIRect &control, int index, int count) const
    {
        const float span = (control.right - control.left) / static_cast<float>(std::max(count, 1));
        return sky::ui::UIRect{control.left + span * static_cast<float>(index) + 1.0f, control.top,
                               control.left + span * static_cast<float>(index + 1) - 2.0f, control.bottom};
    }

    void ReflectedFormView::ComputeHeaderRects()
    {
        const UiMetrics      &m = skin.Theme().metrics;
        const sky::ui::UIRect b = GetBounds();
        const sky::ui::UIRect header{b.left, b.top, b.right, b.top + m.headerHeight};
        const std::string     hint  = "ctrl+Z/Y undo";
        const float           hintW = uc::TextWidth(hint, skin.Theme().fonts.small, textSystem);
        hintRect                    = sky::ui::UIRect{header.right - m.padX - hintW, header.top, header.right - m.padX, header.bottom};
        const float extraW          = ExtraHeaderWidth();
        headerExtraRect             = sky::ui::UIRect{hintRect.left - 10.0f - extraW, header.top + 5.0f, hintRect.left - 10.0f, header.bottom - 5.0f};
        headerTitleRect             = sky::ui::UIRect{header.left + m.padX, header.top, headerExtraRect.left - 10.0f, header.bottom};
    }

    void ReflectedFormView::DrawHeader(sky::ui::UIPaintContext &context, const sky::ui::UIRect &bounds)
    {
        const UiTheme &th = skin.Theme();
        // Same flat header as UiSkin::DrawPanel (UE/Blender style).
        const sky::ui::UIRect header{bounds.left, bounds.top, bounds.right, bounds.top + th.metrics.headerHeight};
        skin.DrawPanelHeader(context, header);
        ComputeHeaderRects();
        uc::Text(context, title, th.fonts.title, headerTitleRect, th.colors.textMuted, textSystem);
        uc::Text(context, "ctrl+Z/Y undo", th.fonts.small, hintRect, th.colors.textMuted, textSystem, uc::HAlign::Right);
        if (ExtraHeaderWidth() > 0.0f) {
            PaintExtraHeader(context, headerExtraRect);
        }
    }

    void ReflectedFormView::DrawSection(sky::ui::UIPaintContext &context, FormSection &section, float &y)
    {
        const UiMetrics      &m = skin.Theme().metrics;
        const sky::ui::UIRect rect{layoutLeft, y, layoutRight, y + m.sectionHeight - 2.0f};
        skin.DrawSectionHeader(context, rect, section.title);
        y += m.sectionHeight;
        for (auto &field : section.fields) {
            DrawField(context, field, y);
        }
        y += 8.0f;
    }

    void ReflectedFormView::DrawField(sky::ui::UIPaintContext &context, PropertyField &field, float &y)
    {
        const UiTheme   &th  = skin.Theme();
        const UiMetrics &m   = th.metrics;
        const Row       *row = RowOf(&field);
        if (row == nullptr) {
            y += m.rowHeight;
            return;
        }

        RowState state = row->alt ? RowState::Alt : RowState::Normal;
        if (editField == &field) {
            state = RowState::Selected;
        } else if (hoverField == &field) {
            state = RowState::Hover;
        }
        skin.DrawRow(context, row->rowRect, state);

        for (int d = 0; d < row->depth; ++d) {
            const float gx = layoutLeft + m.indentX * static_cast<float>(d) + 5.0f;
            context.AddRect(sky::ui::UIRect{gx, row->rowRect.top + 3.0f, gx + 1.0f, row->rowRect.bottom - 3.0f}, th.colors.guide);
        }

        float labelLeft = row->labelRect.left;
        if (field.isStruct || field.isSequence) {
            skin.DrawTriangle(context, labelLeft, (row->rowRect.top + row->rowRect.bottom) * 0.5f, field.expanded, th.colors.text);
            labelLeft += 12.0f;
        }
        const uint32_t labelColor = field.control.readOnly ? th.colors.textDisabled : th.colors.text;
        uc::Text(context, field.label, th.fonts.label, sky::ui::UIRect{labelLeft, row->rowRect.top, row->labelRect.right, row->rowRect.bottom},
                 labelColor, textSystem);

        if (editField == &field) {
            DrawEditBox(context, field, row->controlRect);
        } else if (auto widget = widgets.FindShared(field.kind); widget != nullptr) {
            widget->Paint(*this, context, field, row->controlRect);
        } else {
            switch (field.kind) {
            case PropertyEditorKind::Struct:
            case PropertyEditorKind::Sequence: DrawContainer(context, field, row->controlRect); break;
            default: DrawScalar(context, field, row->controlRect); break;
            }
        }

        if (form.IsModified(field) && !field.control.readOnly) {
            DrawRevertIcon(context, row->revertRect, hoverField == &field);
        }

        y += (row->rowRect.bottom - row->rowRect.top);
        if ((field.isStruct || field.isSequence) && field.expanded) {
            for (auto &child : field.children) {
                DrawField(context, child, y);
            }
        }
    }

    void ReflectedFormView::DrawRevertIcon(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, bool hovered)
    {
        const UiTheme &th = skin.Theme();
        if (hovered) {
            uc::RoundedRect(context, rect, th.colors.rowHover, 3.0f);
        }
        const float    cx       = (rect.left + rect.right) * 0.5f;
        const float    cy       = (rect.top + rect.bottom) * 0.5f;
        const float    radius   = 5.0f;
        const uint32_t color    = hovered ? th.colors.text : th.colors.textMuted;
        const int      segments = 16;
        for (int i = 0; i <= segments; ++i) {
            const float angle = 1.2f + (5.4f - 1.2f) * static_cast<float>(i) / static_cast<float>(segments);
            const float x     = cx + std::cos(angle) * radius;
            const float y     = cy + std::sin(angle) * radius;
            context.AddRect(sky::ui::UIRect{x - 1.0f, y - 1.0f, x + 1.0f, y + 1.0f}, color);
        }
        const float hx = cx + std::cos(1.2f) * radius;
        const float hy = cy + std::sin(1.2f) * radius;
        context.AddRect(sky::ui::UIRect{hx - 2.0f, hy - 3.0f, hx + 2.0f, hy + 1.0f}, color);
    }

    void ReflectedFormView::DrawContainer(sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect)
    {
        const UiTheme &th = skin.Theme();
        if (!field.isSequence) {
            return;
        }
        const uint32_t count = field.descriptor.GetSequenceCount();
        uc::Text(context, std::to_string(count) + " item(s)", th.fonts.value, sky::ui::UIRect{rect.left, rect.top, rect.right, rect.bottom},
                 th.colors.textMuted, textSystem);

        if (!field.control.readOnly) {
            const float           btn = 16.0f;
            const sky::ui::UIRect plus{rect.right - btn, rect.top, rect.right, rect.bottom};
            const sky::ui::UIRect minus{plus.left - btn - 4.0f, rect.top, plus.left - 4.0f, rect.bottom};
            skin.DrawField(context, minus, false, false);
            skin.DrawField(context, plus, false, false);
            uc::Text(context, "-", th.fonts.value, minus, th.colors.text, textSystem, uc::HAlign::Center);
            uc::Text(context, "+", th.fonts.value, plus, th.colors.text, textSystem, uc::HAlign::Center);
        }
    }

    void ReflectedFormView::DrawScalar(sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect)
    {
        const UiTheme &th      = skin.Theme();
        const bool     editing = editField == &field;

        if (field.kind == PropertyEditorKind::Bool) {
            const Any             value = field.descriptor.GetValue();
            const bool            on    = value.GetAsConst<bool>() != nullptr && *value.GetAsConst<bool>();
            const float           side  = th.metrics.checkboxSize;
            const float           cy    = (rect.top + rect.bottom) * 0.5f;
            const sky::ui::UIRect box{rect.left, cy - side * 0.5f, rect.left + side, cy + side * 0.5f};
            skin.DrawCheckbox(context, box, on, hoverField == &field);
            uc::Text(context, on ? "true" : "false", th.fonts.value, sky::ui::UIRect{box.right + 8.0f, rect.top, rect.right, rect.bottom},
                     field.control.readOnly ? th.colors.textDisabled : th.colors.textMuted, textSystem);
            return;
        }

        if (field.control.hasRange && !editing && field.kind != PropertyEditorKind::String) {
            const sky::ui::UIRect slider = SliderRect(rect);
            double                v      = 0.0;
            const Any             value  = field.descriptor.GetValue();
            if (const float *f = value.GetAsConst<float>()) {
                v = *f;
            } else if (const int32_t *i = value.GetAsConst<int32_t>()) {
                v = *i;
            }
            if (dragField == &field) {
                v = dragValue;
            }
            skin.DrawSlider(context, slider, static_cast<float>(v), static_cast<float>(field.control.rangeMin),
                            static_cast<float>(field.control.rangeMax));
            const sky::ui::UIRect valueBox{slider.right + 6.0f, rect.top, rect.right, rect.bottom};
            char                  buffer[32] = {0};
            if (field.kind == PropertyEditorKind::Integer) {
                std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(v));
            } else {
                std::snprintf(buffer, sizeof(buffer), "%.2f", v);
            }
            skin.DrawField(context, valueBox, false, false);
            uc::Text(context, buffer, th.fonts.value, valueBox, th.colors.text, textSystem, uc::HAlign::Center);
            return;
        }

        const sky::ui::UIRect box = rect;
        skin.DrawField(context, box, editing, editInvalid);

        const float textLeft  = box.left + 6.0f;
        const float textRight = (field.kind == PropertyEditorKind::Enum) ? box.right - 18.0f : box.right - 5.0f;
        if (editing) {
            uc::Text(context, editText, th.fonts.value, sky::ui::UIRect{textLeft, box.top, textRight, box.bottom}, th.colors.text, textSystem,
                     uc::HAlign::Left, uc::VAlign::Middle, false);
            const float caretX = textLeft + uc::TextWidth(editText.substr(0, editCaret), th.fonts.value, textSystem);
            context.AddRect(sky::ui::UIRect{caretX, box.top + 3.0f, caretX + 1.0f, box.bottom - 3.0f}, th.colors.text);
        } else {
            const uint32_t textColor = field.control.readOnly ? th.colors.textDisabled : th.colors.text;
            uc::Text(context, FormatValueText(field.descriptor, field.control), th.fonts.value,
                     sky::ui::UIRect{textLeft, box.top, textRight, box.bottom}, textColor, textSystem);
            if (field.kind == PropertyEditorKind::Enum && !field.control.enumNames.empty()) {
                skin.DrawTriangle(context, box.right - 13.0f, (box.top + box.bottom) * 0.5f, true, th.colors.textMuted);
            }
        }
    }

    void ReflectedFormView::BeginInteraction(const Row &row, float x, float y)
    {
        PropertyField &field = *row.field;

        if (!field.control.readOnly && row.revertRect.Contains(x, y)) {
            if (form.IsModified(field) && form.ResetToDefault(field, *commands)) {
                Refresh();
            }
            return;
        }

        if (row.labelRect.Contains(x, y)) {
            if (field.isStruct || field.isSequence) {
                field.expanded = !field.expanded;
                MarkLayoutDirty();
            }
            return;
        }
        if (field.control.readOnly) {
            return;
        }

        // Only container behavior remains in the view; leaf kinds are widgets.
        switch (field.kind) {
        case PropertyEditorKind::Struct:
            field.expanded = !field.expanded;
            MarkLayoutDirty();
            return;
        case PropertyEditorKind::Sequence: HandleSequenceButton(field, row.controlRect, x); return;
        default: return;
        }
    }

    void ReflectedFormView::HandleSequenceButton(PropertyField &field, const sky::ui::UIRect &rect, float x)
    {
        const float           btn = 16.0f;
        const sky::ui::UIRect plus{rect.right - btn, rect.top, rect.right, rect.bottom};
        const sky::ui::UIRect minus{plus.left - btn - 4.0f, rect.top, plus.left - 4.0f, rect.bottom};
        if (plus.Contains(x, plus.top)) {
            const std::string path = field.path;
            if (!form.AddSequenceElement(field, *commands)) {
                return;
            }
            Refresh();
            const auto expand = [&path](auto &self, std::vector<PropertyField> &fields) -> bool {
                for (auto &rebuilt : fields) {
                    if (rebuilt.path == path) {
                        rebuilt.expanded = true;
                        return true;
                    }
                    if (self(self, rebuilt.children)) {
                        return true;
                    }
                }
                return false;
            };
            for (auto &section : form.GetSections()) {
                if (expand(expand, section.fields)) {
                    break;
                }
            }
            MarkLayoutDirty();
        } else if (minus.Contains(x, minus.top)) {
            const int32_t count = static_cast<int32_t>(field.descriptor.GetSequenceCount());
            if (count > 0) {
                if (!form.RemoveSequenceElement(field, static_cast<uint32_t>(count - 1), *commands)) {
                    return;
                }
                Refresh();
                MarkLayoutDirty();
            }
        }
    }

    void ReflectedFormView::CommitEdit()
    {
        if (editField == nullptr) {
            return;
        }
        const std::string text = editText;
        if (editCommit) {
            const std::function<bool(const std::string &)> commit = editCommit;
            if (!commit(text)) {
                editInvalid = true;
                MarkPaintDirty();
                return;
            }
        }

        editField     = nullptr;
        editComponent = -1;
        editInvalid   = false;
        editCommit    = nullptr;
        editText.clear();
        editCaret = 0;
        Refresh();
    }

    void ReflectedFormView::CancelEdit()
    {
        editField     = nullptr;
        editComponent = -1;
        editInvalid   = false;
        editCommit    = nullptr;
        editText.clear();
        editCaret = 0;
        MarkPaintDirty();
    }

    void ReflectedFormView::BeginTextEdit(PropertyField                           &field,
                                          int                                      component,
                                          PropertyEditorKind                       inputKind,
                                          const std::string                       &initial,
                                          std::function<bool(const std::string &)> commit)
    {
        editField     = &field;
        editComponent = component;
        editKind      = inputKind;
        editInvalid   = false;
        // Inline callbacks may capture their widget's this pointer. Keep its
        // creating instance alive after pointer release and registry replacement.
        auto       owner    = dispatchWidget ? dispatchWidget : widgets.FindShared(field.kind);
        const auto lifetime = BindingLifetime();
        editCommit          = [owner = std::move(owner), callback = std::move(commit), lifetime](const std::string &value) {
            (void)owner;
            return !lifetime.expired() && (!callback || callback(value));
        };
        editText  = initial;
        editCaret = editText.size();
        MarkPaintDirty();
    }

    void ReflectedFormView::DrawEditBox(sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect)
    {
        (void)field;
        const UiTheme &th = skin.Theme();
        skin.DrawField(context, rect, true, editInvalid);
        const float textLeft = rect.left + 6.0f;
        uc::Text(context, editText, th.fonts.value, sky::ui::UIRect{textLeft, rect.top, rect.right - 5.0f, rect.bottom}, th.colors.text, textSystem,
                 uc::HAlign::Left, uc::VAlign::Middle, false);
        const float caretX = textLeft + uc::TextWidth(editText.substr(0, editCaret), th.fonts.value, textSystem);
        context.AddRect(sky::ui::UIRect{caretX, rect.top + 3.0f, caretX + 1.0f, rect.bottom - 3.0f}, th.colors.text);
    }

    int ReflectedFormView::WidgetRowCount(const PropertyField &field) const
    {
        if (auto widget = widgets.FindShared(field.kind); widget != nullptr) {
            return std::max(1, widget->RowCount());
        }
        return 1;
    }

} // namespace sky::editor
