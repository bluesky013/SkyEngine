//
// Created on 2026/10/04.
//

#include <editor/shell/ReflectedFormView.h>
#include <editor/shell/AssetReflectedWidget.h>
#include <editor/shell/ColorReflectedWidget.h>
#include <editor/shell/EnumReflectedWidget.h>
#include <editor/shell/GenericReflectedWidget.h>
#include <editor/shell/RotationReflectedWidget.h>
#include <editor/shell/UiDraw.h>

#include <editor/core/EditorCore.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/property/PropertyValidation.h>

#include <core/math/Quaternion.h>
#include <core/math/Transform.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/type/TypeInfo.h>
#include <core/util/Uuid.h>
#include <framework/serialization/SerializationContext.h>

#include <ui/text/UITextSystem.h>

#include <memory>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

        constexpr float kValueBoxW = 60.0f;
        constexpr uint32_t kKeyBackspace = 0x08;
        constexpr uint32_t kKeyReturn = 0x0D;
        constexpr uint32_t kKeyEscape = 0x1B;
        constexpr uint32_t kKeyLeft = 0x25;
        constexpr uint32_t kKeyRight = 0x27;
        constexpr uint32_t kKeyHome = 0x24;
        constexpr uint32_t kKeyEnd = 0x23;
        constexpr uint32_t kKeyDelete = 0x2E;
        constexpr uint32_t kKeyZ = 0x5A;
        constexpr uint32_t kKeyY = 0x59;

        
        int64_t EnumRaw(const Any &value)
        {
            int64_t out = 0;
            const TypeInfoRT *info = value.Info();
            if (info == nullptr || info->staticInfo == nullptr || value.Data() == nullptr) {
                return 0;
            }
            const size_t size = std::min<size_t>(info->staticInfo->size, sizeof(out));
            std::memcpy(&out, value.Data(), size);
            return out;
        }

        Any MakeEnumValue(const TypeInfoRT *info, int64_t raw)
        {
            if (info == nullptr || info->staticInfo == nullptr) {
                return {};
            }
            uint8_t buffer[8] = {0};
            const size_t size = std::min<size_t>(info->staticInfo->size, sizeof(buffer));
            std::memcpy(buffer, &raw, size);
            return Any::Create(info, buffer);
        }

        std::string FloatMemberLabel(const PropertyDescriptor &descriptor, int index)
        {
            const TypeNode *node = descriptor.GetStructType();
            if (node == nullptr) {
                return {};
            }
            int idx = 0;
            for (const auto &entry : node->members) {
                const TypeInfoRT *info = entry.second.info;
                if (info == nullptr || info->staticInfo == nullptr || !info->staticInfo->isFloatingPoint) {
                    continue;
                }
                if (idx == index) {
                    std::string label(entry.first);
                    if (!label.empty()) {
                        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
                    }
                    return label;
                }
                ++idx;
            }
            return {};
        }

        
        std::string FormatValueText(const PropertyDescriptor &descriptor, const EditorControl &control)
        {
            const Any value = descriptor.GetValue();
            const TypeInfoRT *info = value.Info();
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

            char buffer[64] = {0};
            const int decimals = DecimalsForControl(control);
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
        : textSystem(text)
        , title(std::move(inTitle))
        , commands(&EditorCore::GetCommandService())
        , registry(&EditorCore::GetPropertyEditors())
        , skin(GetDefaultUiTheme(), text)
    {
        SetFocusable(true);
        static bool registered = false;
        if (!registered) {
            registered = true;
            ReflectedWidgetRegistry &widgets = ReflectedWidgetRegistry::Get();
            widgets.Register(PropertyEditorKind::Color, std::make_shared<ColorReflectedWidget>());
            widgets.Register(PropertyEditorKind::Rotation, std::make_shared<RotationReflectedWidget>());
            widgets.Register(PropertyEditorKind::Asset, std::make_shared<AssetReflectedWidget>());
            widgets.Register(PropertyEditorKind::Enum, std::make_shared<EnumReflectedWidget>());
            auto generic = std::make_shared<GenericReflectedWidget>();
            for (PropertyEditorKind kind : {PropertyEditorKind::Bool, PropertyEditorKind::Integer,
                                            PropertyEditorKind::Float, PropertyEditorKind::String,
                                            PropertyEditorKind::Vector}) {
                widgets.Register(kind, generic);
            }

            // Engine math types -> editor kinds (metadata, not view type checks).
            registry->RegisterType(TypeInfo<Vector2>::RegisteredId(), PropertyEditorKind::Vector);
            registry->RegisterType(TypeInfo<Vector3>::RegisteredId(), PropertyEditorKind::Vector);
            registry->RegisterType(TypeInfo<Vector4>::RegisteredId(), PropertyEditorKind::Vector);
            registry->RegisterType(TypeInfo<Quaternion>::RegisteredId(), PropertyEditorKind::Rotation);

            // Transform presents its data as Position / Rotation / Scale (T/R/S).
            const Uuid transformType = TypeInfo<Transform>::RegisteredId();
            registry->RegisterMemberAppearance(transformType, "translation", {"Position", 0, true});
            registry->RegisterMemberAppearance(transformType, "rotation", {"Rotation", 1, true});
            registry->RegisterMemberAppearance(transformType, "scale", {"Scale", 2, true});
        }
    }

    ReflectedFormView::~ReflectedFormView() = default;

    CommandService &ReflectedFormView::Commands() { return *commands; }
    void ReflectedFormView::MarkDirty() { MarkPaintDirty(); }
    void ReflectedFormView::RefreshForm() { Refresh(); }

    void ReflectedFormView::Bind(const PropertyObject &object)
    {
        form.Build(object, *registry);
        MarkPaintDirty();
    }

    void ReflectedFormView::Refresh()
    {
        form.Rebuild();
        MarkPaintDirty();
    }

    void ReflectedFormView::OnPaint(sky::ui::UIPaintContext &context)
    {
        PollExternalChanges();
        OnViewTick();

        const UiTheme &th = skin.Theme();
        const sky::ui::UIRect bounds = GetBounds();
        uc::Fill(context, bounds, th.colors.panel);
        DrawHeader(context, bounds);
        BuildRows();

        const sky::ui::UIRect content{bounds.left, bounds.top + th.metrics.headerHeight, bounds.right, bounds.bottom};
        context.PushClip(content);
        float y = content.top + 4.0f - scroll;
        for (auto &section : form.GetSections()) {
            DrawSection(context, section, y);
        }
        contentHeight = (y + scroll) - content.top + 8.0f;
        context.PopClip();

        skin.DrawScrollbar(context, content, contentHeight, scroll);
        if (ReflectedWidget *popup = ReflectedWidgetRegistry::Get().FindWithPopup(); popup != nullptr) {
            popup->PaintPopup(*this, context);
        }
    }

    sky::ui::UIEventResult ReflectedFormView::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        BuildRows();
        if (ReflectedWidget *popup = ReflectedWidgetRegistry::Get().FindWithPopup(); popup != nullptr) {
            return popup->OnPopupPointer(*this, event) ? sky::ui::UIEventResult::HANDLED
                                                       : sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::WHEEL) {
            const UiMetrics &m = skin.Theme().metrics;
            const float viewH = std::max(0.0f, GetBounds().bottom - GetBounds().top - m.headerHeight);
            scroll = std::clamp(scroll - event.wheelDelta * 0.6f, 0.0f, std::max(0.0f, contentHeight - viewH));
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::MOVE) {
            if (activeWidget != nullptr) {
                activeWidget->OnMove(*this, event);
                return sky::ui::UIEventResult::HANDLED;
            }
            const Row *row = RowAt(event.x, event.y);
            PropertyField *hover = row != nullptr ? row->field : nullptr;
            if (hover != hoverField) {
                hoverField = hover;
                MarkPaintDirty();
            }
            return hoverField != nullptr ? sky::ui::UIEventResult::HANDLED : sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::UP) {
            if (activeWidget != nullptr) {
                activeWidget->OnUp(*this, event);
                activeWidget = nullptr;
                return sky::ui::UIEventResult::HANDLED;
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        ComputeHeaderRects();
        if (const sky::ui::UIEventResult extra = HandleExtraHeaderPointer(event);
            extra != sky::ui::UIEventResult::UNHANDLED) {
            return extra;
        }

        const Row *row = RowAt(event.x, event.y);
        if (row == nullptr) {
            if (editField != nullptr) {
                CommitEdit();
            }
            return editField != nullptr ? sky::ui::UIEventResult::HANDLED : sky::ui::UIEventResult::UNHANDLED;
        }
        if (ReflectedWidget *widget = ReflectedWidgetRegistry::Get().Find(row->field->kind); widget != nullptr) {
            if (widget->OnDown(*this, *row->field, event, row->controlRect)) {
                activeWidget = widget;
                return sky::ui::UIEventResult::HANDLED;
            }
        }
        BeginInteraction(*row, event.x, event.y);
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult ReflectedFormView::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
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
            case kKeyReturn:
                CommitEdit();
                return sky::ui::UIEventResult::HANDLED;
            case kKeyEscape:
                CancelEdit();
                return sky::ui::UIEventResult::HANDLED;
            default:
                return sky::ui::UIEventResult::HANDLED;
            }
        }

        if (event.keyCode == kKeyEscape) {
            if (ReflectedWidget *popup = ReflectedWidgetRegistry::Get().FindWithPopup(); popup != nullptr) {
                popup->OnEscape(*this);
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
        }
        if (event.keyCode == kKeyZ && event.modifiers != 0) {
            if (commands->Undo()) {
                Refresh();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.keyCode == kKeyY && event.modifiers != 0) {
            if (commands->Redo()) {
                Refresh();
            }
            return sky::ui::UIEventResult::HANDLED;
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
        const uint64_t revision = EditorCore::GetPropertyChanges().GetRevision();
        if (revision == lastRevision) {
            return;
        }
        lastRevision = revision;
        if (editField == nullptr && dragField == nullptr &&
            ReflectedWidgetRegistry::Get().FindWithPopup() == nullptr) {
            form.Rebuild();
            MarkPaintDirty();
        } else {
            pendingExternalRefresh = true;
        }
    }

    void ReflectedFormView::BuildRows()
    {
        rows.clear();
        const UiMetrics &m = skin.Theme().metrics;
        const sky::ui::UIRect bounds = GetBounds();
        layoutLeft = bounds.left + m.padX;
        layoutRight = bounds.right - m.padX - m.scrollBarWidth;
        labelWidth = std::min(240.0f, (layoutRight - layoutLeft) * 0.42f);

        float y = bounds.top + m.headerHeight + 4.0f - scroll;
        bool alt = false;
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
        const UiMetrics &m = skin.Theme().metrics;
        const float indent = m.indentX * static_cast<float>(depth);
        const float h = m.rowHeight * static_cast<float>(std::max(1, WidgetRowCount(field)));
        Row row;
        row.field = &field;
        row.depth = depth;
        row.alt = alt;
        row.rowRect = sky::ui::UIRect{layoutLeft, y, layoutRight, y + h};
        row.labelRect = sky::ui::UIRect{layoutLeft + indent + 4.0f, y, layoutLeft + indent + labelWidth, y + h};
        row.revertRect = sky::ui::UIRect{layoutRight - 16.0f, y + 1.0f, layoutRight, y + h - 1.0f};
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
        const UiMetrics &m = skin.Theme().metrics;
        const sky::ui::UIRect b = GetBounds();
        const sky::ui::UIRect header{b.left, b.top, b.right, b.top + m.headerHeight};
        const std::string hint = "ctrl+Z/Y undo";
        const float hintW = uc::TextWidth(hint, skin.Theme().fonts.small, textSystem);
        hintRect = sky::ui::UIRect{header.right - m.padX - hintW, header.top, header.right - m.padX, header.bottom};
        const float extraW = ExtraHeaderWidth();
        headerExtraRect = sky::ui::UIRect{hintRect.left - 10.0f - extraW, header.top + 5.0f, hintRect.left - 10.0f, header.bottom - 5.0f};
        headerTitleRect = sky::ui::UIRect{header.left + m.padX, header.top, headerExtraRect.left - 10.0f, header.bottom};
    }

    void ReflectedFormView::DrawHeader(sky::ui::UIPaintContext &context, const sky::ui::UIRect &bounds)
    {
        const UiTheme &th = skin.Theme();
        const sky::ui::UIRect header{bounds.left, bounds.top, bounds.right, bounds.top + th.metrics.headerHeight};
        uc::RoundedGradient(context, header, th.colors.headerTop, th.colors.header, 0.0f);
        uc::HLine(context, header.left, header.right, header.bottom - 1.0f, th.colors.accent);
        ComputeHeaderRects();
        uc::Text(context, title, th.fonts.title, headerTitleRect, th.colors.text, textSystem);
        uc::Text(context, "ctrl+Z/Y undo", th.fonts.small, hintRect, th.colors.textMuted, textSystem, uc::HAlign::Right);
        if (ExtraHeaderWidth() > 0.0f) {
            PaintExtraHeader(context, headerExtraRect);
        }
    }

    void ReflectedFormView::DrawSection(sky::ui::UIPaintContext &context, FormSection &section, float &y)
    {
        const UiMetrics &m = skin.Theme().metrics;
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
        const UiTheme &th = skin.Theme();
        const UiMetrics &m = th.metrics;
        const Row *row = RowOf(&field);
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
        uc::Text(context, field.label, th.fonts.label, sky::ui::UIRect{labelLeft, row->rowRect.top, row->labelRect.right, row->rowRect.bottom}, labelColor, textSystem);

        if (editField == &field) {
            DrawEditBox(context, field, row->controlRect);
        } else if (ReflectedWidget *widget = ReflectedWidgetRegistry::Get().Find(field.kind); widget != nullptr) {
            widget->Paint(*this, context, field, row->controlRect);
        } else {
            switch (field.kind) {
            case PropertyEditorKind::Struct:
            case PropertyEditorKind::Sequence:
                DrawContainer(context, field, row->controlRect);
                break;
            default:
                DrawScalar(context, field, row->controlRect);
                break;
            }
        }

        if (form.IsModified(field)) {
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
        const float cx = (rect.left + rect.right) * 0.5f;
        const float cy = (rect.top + rect.bottom) * 0.5f;
        const float radius = 5.0f;
        const uint32_t color = hovered ? th.colors.text : th.colors.textMuted;
        const int segments = 16;
        for (int i = 0; i <= segments; ++i) {
            const float angle = 1.2f + (5.4f - 1.2f) * static_cast<float>(i) / static_cast<float>(segments);
            const float x = cx + std::cos(angle) * radius;
            const float y = cy + std::sin(angle) * radius;
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
        uc::Text(context, std::to_string(count) + " item(s)", th.fonts.value, sky::ui::UIRect{rect.left, rect.top, rect.right, rect.bottom}, th.colors.textMuted, textSystem);

        if (!field.control.readOnly) {
            const float btn = 16.0f;
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
        const UiTheme &th = skin.Theme();
        const bool editing = editField == &field;

        if (field.kind == PropertyEditorKind::Bool) {
            const Any value = field.descriptor.GetValue();
            const bool on = value.GetAsConst<bool>() != nullptr && *value.GetAsConst<bool>();
            const float side = th.metrics.checkboxSize;
            const float cy = (rect.top + rect.bottom) * 0.5f;
            const sky::ui::UIRect box{rect.left, cy - side * 0.5f, rect.left + side, cy + side * 0.5f};
            skin.DrawCheckbox(context, box, on, hoverField == &field);
            uc::Text(context, on ? "true" : "false", th.fonts.value,
                     sky::ui::UIRect{box.right + 8.0f, rect.top, rect.right, rect.bottom},
                     field.control.readOnly ? th.colors.textDisabled : th.colors.textMuted, textSystem);
            return;
        }

        if (field.control.hasRange && !editing && field.kind != PropertyEditorKind::String) {
            const sky::ui::UIRect slider = SliderRect(rect);
            double v = 0.0;
            const Any value = field.descriptor.GetValue();
            if (const float *f = value.GetAsConst<float>()) { v = *f; }
            else if (const int32_t *i = value.GetAsConst<int32_t>()) { v = *i; }
            if (dragField == &field) { v = dragValue; }
            skin.DrawSlider(context, slider, static_cast<float>(v), static_cast<float>(field.control.rangeMin), static_cast<float>(field.control.rangeMax));
            const sky::ui::UIRect valueBox{slider.right + 6.0f, rect.top, rect.right, rect.bottom};
            char buffer[32] = {0};
            std::snprintf(buffer, sizeof(buffer), (field.kind == PropertyEditorKind::Integer) ? "%d" : "%.2f", (field.kind == PropertyEditorKind::Integer) ? static_cast<int>(v) : v);
            skin.DrawField(context, valueBox, false, false);
            uc::Text(context, buffer, th.fonts.value, valueBox, th.colors.text, textSystem, uc::HAlign::Center);
            return;
        }

        const sky::ui::UIRect box = rect;
        skin.DrawField(context, box, editing, editInvalid);

        const float textLeft = box.left + 6.0f;
        const float textRight = (field.kind == PropertyEditorKind::Enum) ? box.right - 18.0f : box.right - 5.0f;
        if (editing) {
            uc::Text(context, editText, th.fonts.value, sky::ui::UIRect{textLeft, box.top, textRight, box.bottom}, th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
            const float caretX = textLeft + uc::TextWidth(editText.substr(0, editCaret), th.fonts.value, textSystem);
            context.AddRect(sky::ui::UIRect{caretX, box.top + 3.0f, caretX + 1.0f, box.bottom - 3.0f}, th.colors.text);
        } else {
            const uint32_t textColor = field.control.readOnly ? th.colors.textDisabled : th.colors.text;
            uc::Text(context, FormatValueText(field.descriptor, field.control), th.fonts.value, sky::ui::UIRect{textLeft, box.top, textRight, box.bottom}, textColor, textSystem);
            if (field.kind == PropertyEditorKind::Enum && !field.control.enumNames.empty()) {
                skin.DrawTriangle(context, box.right - 13.0f, (box.top + box.bottom) * 0.5f, true, th.colors.textMuted);
            }
        }
    }

    void ReflectedFormView::BeginInteraction(const Row &row, float x, float y)
    {
        PropertyField &field = *row.field;

        if (editField != nullptr && editField != &field) {
            CommitEdit();
        }

        if (row.revertRect.Contains(x, y)) {
            if (form.IsModified(field) && form.ResetToDefault(field, *commands)) {
                Refresh();
            }
            return;
        }

        if (row.labelRect.Contains(x, y)) {
            if (field.isStruct || field.isSequence) {
                field.expanded = !field.expanded;
                MarkPaintDirty();
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
            MarkPaintDirty();
            return;
        case PropertyEditorKind::Sequence:
            HandleSequenceButton(field, row.controlRect, x);
            return;
        default:
            return;
        }
    }

    void ReflectedFormView::HandleSequenceButton(PropertyField &field, const sky::ui::UIRect &rect, float x)
    {
        const float btn = 16.0f;
        const sky::ui::UIRect plus{rect.right - btn, rect.top, rect.right, rect.bottom};
        const sky::ui::UIRect minus{plus.left - btn - 4.0f, rect.top, plus.left - 4.0f, rect.bottom};
        if (plus.Contains(x, plus.top)) {
            commands->Execute(field.descriptor.MakeAddSequenceElementCommand());
            form.Rebuild();
            field.expanded = true;
            MarkPaintDirty();
        } else if (minus.Contains(x, minus.top)) {
            const int32_t count = static_cast<int32_t>(field.descriptor.GetSequenceCount());
            if (count > 0) {
                commands->Execute(field.descriptor.MakeRemoveSequenceElementCommand(static_cast<uint32_t>(count - 1)));
                form.Rebuild();
                MarkPaintDirty();
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

        editField = nullptr;
        editComponent = -1;
        editInvalid = false;
        editCommit = nullptr;
        editText.clear();
        editCaret = 0;
        Refresh();
    }

    void ReflectedFormView::CancelEdit()
    {
        editField = nullptr;
        editComponent = -1;
        editInvalid = false;
        editCommit = nullptr;
        editText.clear();
        editCaret = 0;
        MarkPaintDirty();
    }

    void ReflectedFormView::BeginTextEdit(PropertyField &field, int component, PropertyEditorKind inputKind,
                                          const std::string &initial, std::function<bool(const std::string &)> commit)
    {
        editField = &field;
        editComponent = component;
        editKind = inputKind;
        editInvalid = false;
        editCommit = std::move(commit);
        editText = initial;
        editCaret = editText.size();
        MarkPaintDirty();
    }

    void ReflectedFormView::DrawEditBox(sky::ui::UIPaintContext &context, PropertyField &field,
                                        const sky::ui::UIRect &rect)
    {
        (void)field;
        const UiTheme &th = skin.Theme();
        skin.DrawField(context, rect, true, editInvalid);
        const float textLeft = rect.left + 6.0f;
        uc::Text(context, editText, th.fonts.value, sky::ui::UIRect{textLeft, rect.top, rect.right - 5.0f, rect.bottom},
                 th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        const float caretX = textLeft + uc::TextWidth(editText.substr(0, editCaret), th.fonts.value, textSystem);
        context.AddRect(sky::ui::UIRect{caretX, rect.top + 3.0f, caretX + 1.0f, rect.bottom - 3.0f}, th.colors.text);
    }

    int ReflectedFormView::WidgetRowCount(const PropertyField &field) const
    {
        if (ReflectedWidget *widget = ReflectedWidgetRegistry::Get().Find(field.kind); widget != nullptr) {
            return std::max(1, widget->RowCount());
        }
        return 1;
    }

} // namespace sky::editor
