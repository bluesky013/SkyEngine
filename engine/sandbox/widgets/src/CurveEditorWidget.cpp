#include <editor/widgets/CurveEditorWidget.h>

#include <editor/core/EditorCore.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/UiSkin.h>
#include <framework/serialization/SerializationContext.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/ListView.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace sky::editor {
    std::span<const CurveEditorCommandInfo> CurveEditorCommands()
    {
        static const CurveEditorCommandInfo commands[] = {
            {CurveEditorCommand::AddKey, "curve.addKey", "Add key", "speed-up", 30, ""},
            {CurveEditorCommand::RemoveSelected, "curve.removeKeys", "Remove keys", "speed-down", 31, "Delete,Backspace"},
            {CurveEditorCommand::Duplicate, "curve.duplicate", "Duplicate keys", "speed-up", 32, "Ctrl+D"},
            {CurveEditorCommand::Copy, "curve.copy", "Copy keys", "speed-up", 33, "Ctrl+C"},
            {CurveEditorCommand::Paste, "curve.paste", "Paste keys", "speed-up", 34, "Ctrl+V"},
            {CurveEditorCommand::FrameAll, "curve.frameAll", "Frame all", "focus", 35, "F"},
            {CurveEditorCommand::ToggleSnap, "curve.toggleSnap", "Snap", "snap", 36, "S"},
            {CurveEditorCommand::SetLinear, "curve.linear", "Linear", "speed-up", 37, "L"},
            {CurveEditorCommand::SetStep, "curve.step", "Step", "speed-up", 38, "T"},
            {CurveEditorCommand::SetCubic, "curve.cubic", "Cubic", "speed-up", 39, "C"},
        };
        return commands;
    }

    namespace {
        std::string FormatValue(const Vector4 &value, uint32_t channels)
        {
            char buffer[128];
            switch (channels) {
            case 1: std::snprintf(buffer, sizeof(buffer), "%.3f", value.x); break;
            case 2: std::snprintf(buffer, sizeof(buffer), "%.3f, %.3f", value.x, value.y); break;
            case 3: std::snprintf(buffer, sizeof(buffer), "%.3f, %.3f, %.3f", value.x, value.y, value.z); break;
            default: std::snprintf(buffer, sizeof(buffer), "%.3f, %.3f, %.3f, %.3f", value.x, value.y, value.z, value.w); break;
            }
            return std::string(buffer);
        }
        float NiceStep(float raw)
        {
            if (!(raw > 0.f) || !std::isfinite(raw))
                return 1.f;
            const float magnitude  = std::pow(10.f, std::floor(std::log10(raw)));
            const float normalized = raw / magnitude;
            const float rounded    = normalized < 1.5f ? 1.f : normalized < 3.5f ? 2.f : normalized < 7.5f ? 5.f : 10.f;
            return rounded * magnitude;
        }
        bool Contains(const sky::ui::UIRect &rect, float x, float y)
        {
            return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
        }
    } // namespace

    class CurveEditorWidget::KeyList final : public sky::ui::ListView {
        class Row final : public sky::ui::UIElement {
        public:
            Row(CurveEditorWidget &owner, int index) : owner(owner), index(index)
            {
            }
            sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override
            {
                if (event.action != sky::ui::UIPointerAction::DOWN || event.button != 0 || !GetBounds().Contains(event.x, event.y))
                    return sky::ui::UIEventResult::UNHANDLED;
                owner.SetSelection({index}, index);
                return sky::ui::UIEventResult::HANDLED;
            }
            void OnPaint(sky::ui::UIPaintContext &context) override
            {
                if (index >= static_cast<int>(owner.curve.keys.size()))
                    return;
                const auto  &theme = GetDefaultUiTheme();
                const UiSkin skin(theme, owner.text);
                const auto   rect     = GetBounds();
                const bool   selected = std::binary_search(owner.selection.begin(), owner.selection.end(), index);
                skin.DrawRow(context, rect, selected ? RowState::Selected : RowState::Normal);
                if (owner.text) {
                    const auto &key = owner.curve.keys[index];
                    char        label[192];
                    std::snprintf(label, sizeof(label), "%.3f    %s", key.time, FormatValue(key.value, owner.curve.channels).c_str());
                    uidraw::Text(context, label, theme.fonts.label, {rect.left + theme.metrics.controlPad, rect.top, rect.right, rect.bottom},
                                 selected ? theme.colors.textOnAccent : theme.colors.text, owner.text, uidraw::HAlign::Left, uidraw::VAlign::Middle,
                                 true);
                }
            }

        private:
            CurveEditorWidget &owner;
            int                index;
        };

    public:
        explicit KeyList(CurveEditorWidget &owner) : owner(owner)
        {
            SetItemFactory([this](size_t index) { return std::make_unique<Row>(this->owner, static_cast<int>(index)); });
            Refresh();
        }
        void Refresh()
        {
            SetItemCount(owner.curve.keys.size());
            SetItemHeight(GetDefaultUiTheme().metrics.rowHeight);
            for (const auto &row : GetChildren())
                row->MarkPaintDirty();
            MarkPaintDirty();
        }
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override
        {
            if (!GetBounds().Contains(event.x, event.y))
                return sky::ui::UIEventResult::UNHANDLED;
            if (event.action == sky::ui::UIPointerAction::WHEEL) {
                ScrollBy(-event.wheelDelta * GetDefaultUiTheme().metrics.rowHeight);
                return sky::ui::UIEventResult::HANDLED;
            }
            for (const auto &row : GetChildren()) {
                if (row->GetBounds().Contains(event.x, event.y))
                    return row->OnPointerEvent(event);
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }
        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            UiSkin(GetDefaultUiTheme(), owner.text).DrawPanel(context, GetBounds(), {}, false);
        }

    private:
        CurveEditorWidget &owner;
    };

    struct AxisTicks {
        std::vector<float> values;
        float              step      = 1.f;
        int                precision = 0;
    };

    class CurveEditorWidget::Graph final : public sky::ui::UIElement {
    public:
        explicit Graph(CurveEditorWidget &owner) : owner(owner)
        {
        }
        void SetText(sky::ui::UITextSystem *value)
        {
            text = value;
        }
        void SynchronizeModel()
        {
            activeChannel = std::min(activeChannel, std::clamp(owner.curve.channels, 1u, 4u) - 1);
            MarkPaintDirty();
        }
        void FrameAll()
        {
            timeStart = 0.f;
            timeSpan  = 1.f;
            MarkPaintDirty();
        }
        bool Editing() const
        {
            return mode != Mode::None;
        }
        void Cancel()
        {
            if (mode == Mode::Keys || mode == Mode::TangentIn || mode == Mode::TangentOut) {
                mode        = Mode::None;
                owner.curve = dragStart;
                owner.SetSelection(dragSelection, primary);
            }
            mode = Mode::None;
            MarkPaintDirty();
        }
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override
        {
            owner.SynchronizeCurrent();
            if (mode == Mode::None && !GetBounds().Contains(event.x, event.y))
                return sky::ui::UIEventResult::UNHANDLED;
            if (mode != Mode::None && event.pointerId != pointer)
                return sky::ui::UIEventResult::UNHANDLED;
            const auto plot = Plot();
            switch (event.action) {
            case sky::ui::UIPointerAction::WHEEL: {
                if (mode != Mode::None)
                    return sky::ui::UIEventResult::HANDLED;
                if (plot.Width() <= 0.f)
                    return sky::ui::UIEventResult::HANDLED;
                const float anchor   = TimeAt(event.x);
                timeSpan             = std::clamp(timeSpan * std::exp(-event.wheelDelta * .15f), .02f, 1.f);
                const float relative = (event.x - plot.left) / plot.Width();
                timeStart            = std::clamp(anchor - relative * timeSpan, 0.f, 1.f - timeSpan);
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            case sky::ui::UIPointerAction::DOWN:
                if (mode != Mode::None)
                    return sky::ui::UIEventResult::HANDLED;
                pointer = event.pointerId;
                if (event.button == 1 || event.button == 2) {
                    mode  = Mode::Pan;
                    lastX = event.x;
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (event.button == 0) {
                    TrackCursor(TimeAt(event.x));
                    SetCursor(event.x, event.y);
                    const int primaryKey = owner.PrimarySelection();
                    if (owner.curve.interpolation == curve::CurveInterpolation::CUBIC && primaryKey >= 0 &&
                        primaryKey < static_cast<int>(owner.curve.keys.size())) {
                        const int handle = HitTangent(primaryKey, activeChannel, event.x, event.y);
                        if (handle != 0) {
                            ValueRange(dragMinimum, dragMaximum);
                            mode          = handle > 0 ? Mode::TangentOut : Mode::TangentIn;
                            tangentKey    = primaryKey;
                            dragStart     = owner.curve;
                            dragSelection = owner.selection;
                            primary       = primaryKey;
                            return sky::ui::UIEventResult::HANDLED;
                        }
                    }
                    const auto hit = HitKey(event.x, event.y);
                    if (hit.key >= 0) {
                        if (std::find(owner.selection.begin(), owner.selection.end(), hit.key) == owner.selection.end())
                            owner.SetSelection({hit.key}, hit.key);
                        else
                            owner.SetSelection(owner.selection, hit.key);
                        activeChannel = hit.channel;
                        primary       = hit.key;
                        dragSelection = owner.selection;
                        dragStart     = owner.curve;
                        anchorTime    = TimeAt(event.x);
                        anchorValue   = ValueAt(event.y);
                        ValueRange(dragMinimum, dragMaximum);
                        mode = Mode::Keys;
                        owner.BindSelection();
                    } else {
                        owner.SetSelection({}, -1);
                        marqueeStart = {event.x, event.y};
                        marqueeEnd   = marqueeStart;
                        mode         = Mode::Marquee;
                    }
                    return sky::ui::UIEventResult::HANDLED;
                }
                break;
            case sky::ui::UIPointerAction::MOVE:
                TrackCursor(TimeAt(event.x));
                SetCursor(event.x, event.y);
                if (mode == Mode::Pan) {
                    timeStart =
                        std::clamp(timeStart - (event.x - lastX) / std::max(GetDefaultUiTheme().scale, plot.Width()) * timeSpan, 0.f, 1.f - timeSpan);
                    lastX = event.x;
                    MarkPaintDirty();
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (mode == Mode::Keys) {
                    DragKeys(event.x, event.y);
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (mode == Mode::TangentIn || mode == Mode::TangentOut) {
                    DragTangent(event.x, event.y);
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (mode == Mode::Marquee) {
                    marqueeEnd = {event.x, event.y};
                    MarkPaintDirty();
                    return sky::ui::UIEventResult::HANDLED;
                }
                break;
            case sky::ui::UIPointerAction::UP:
                if (mode == Mode::None)
                    break;
                if (mode == Mode::Pan) {
                    mode = Mode::None;
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (mode == Mode::Keys) {
                    mode = Mode::None;
                    owner.Commit("Move Curve keys", owner.curve);
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (mode == Mode::TangentIn || mode == Mode::TangentOut) {
                    mode = Mode::None;
                    owner.Commit("Edit Curve tangent", owner.curve);
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (mode == Mode::Marquee) {
                    mode = Mode::None;
                    SelectInMarquee();
                    return sky::ui::UIEventResult::HANDLED;
                }
                break;
            default: break;
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }
        bool WantsPointerCapture() const override
        {
            return mode != Mode::None;
        }
        void OnPointerCancel(uint32_t) override
        {
            Cancel();
        }
        void OnPointerButtonConflict(uint32_t) override
        {
            Cancel();
        }
        void OnPointerLeave(const sky::ui::UIPointerEvent &) override
        {
            hasCursor = false;
            MarkPaintDirty();
        }
        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            const auto &theme = GetDefaultUiTheme();
            const auto  plot  = Plot();
            context.AddRect(GetBounds(), theme.colors.field);
            if (plot.Width() <= 0.f || plot.Height() <= 0.f)
                return;
            float minimum = 0.f, maximum = 0.f;
            ValueRange(minimum, maximum);
            const AxisTicks timeTicks  = MakeTicks(timeStart, timeStart + timeSpan);
            const AxisTicks valueTicks = MakeTicks(minimum, maximum);
            const uint32_t  minorGrid  = theme.colors.guide;
            const uint32_t  majorGrid  = theme.colors.border;
            const uint32_t  axisColor  = theme.colors.textMuted;
            for (const float value : valueTicks.values) {
                const float y = plot.bottom - (value - minimum) / std::max(1e-5f, maximum - minimum) * plot.Height();
                if (y < plot.top - theme.scale || y > plot.bottom + theme.scale)
                    continue;
                context.AddRect({plot.left, y, plot.right, y + theme.scale}, majorGrid);
                if (text) {
                    char label[32];
                    std::snprintf(label, sizeof(label), "%.*f", valueTicks.precision, value);
                    uidraw::Text(context, label, theme.fonts.label,
                                 {plot.left - theme.metrics.sliderValueWidth, y - theme.metrics.rowHeight * .5f,
                                  plot.left - theme.metrics.cellPadding, y + theme.metrics.rowHeight * .5f},
                                 theme.colors.text, text, uidraw::HAlign::Right, uidraw::VAlign::Middle);
                }
            }
            for (const float value : timeTicks.values) {
                const float x = plot.left + (value - timeStart) / timeSpan * plot.Width();
                if (x < plot.left - theme.scale || x > plot.right + theme.scale)
                    continue;
                context.AddRect({x, plot.top, x + theme.scale, plot.bottom}, majorGrid);
                if (text) {
                    char label[32];
                    std::snprintf(label, sizeof(label), "%.*f", timeTicks.precision, value);
                    float left = x - theme.metrics.frameHeight, right = x + theme.metrics.frameHeight;
                    if (right > GetBounds().right - theme.metrics.controlPad) {
                        const float shift = right - (GetBounds().right - theme.metrics.controlPad);
                        left -= shift;
                        right -= shift;
                    }
                    if (left < GetBounds().left + theme.metrics.controlPad)
                        left = GetBounds().left + theme.metrics.controlPad;
                    uidraw::Text(context, label, theme.fonts.label, {left, plot.bottom, right, plot.bottom + theme.metrics.rowHeight},
                                 theme.colors.text, text, uidraw::HAlign::Center, uidraw::VAlign::Top);
                }
                if (timeTicks.step > 0.f) {
                    for (int minor = 1; minor < 5; ++minor) {
                        const float mx = x + (timeTicks.step * minor / 5.f) / timeSpan * plot.Width();
                        if (mx >= plot.left && mx <= plot.right)
                            context.AddRect({mx, plot.top, mx + theme.scale, plot.bottom}, minorGrid);
                    }
                }
            }
            for (const float value : valueTicks.values) {
                if (valueTicks.step <= 0.f)
                    break;
                const float y = plot.bottom - (value - minimum) / std::max(1e-5f, maximum - minimum) * plot.Height();
                for (int minor = 1; minor < 5; ++minor) {
                    const float my = y + (valueTicks.step * minor / 5.f) / std::max(1e-5f, maximum - minimum) * plot.Height();
                    if (my >= plot.top && my <= plot.bottom)
                        context.AddRect({plot.left, my, plot.right, my + theme.scale}, minorGrid);
                }
            }
            context.AddLine(plot.left, plot.top, plot.left, plot.bottom, theme.metrics.selectionWidth * .6f, axisColor);
            context.AddLine(plot.left, plot.bottom, plot.right, plot.bottom, theme.metrics.selectionWidth * .6f, axisColor);
            if (text) {
                uidraw::Text(context, "value", theme.fonts.label,
                             {GetBounds().left + theme.metrics.controlPad, plot.top - theme.metrics.rowHeight, plot.left - theme.metrics.cellPadding,
                              plot.top},
                             theme.colors.textMuted, text, uidraw::HAlign::Right, uidraw::VAlign::Middle);
                uidraw::Text(context, "time", theme.fonts.label, {plot.left, plot.bottom, plot.right, plot.bottom + theme.metrics.rowHeight},
                             theme.colors.textMuted, text, uidraw::HAlign::Right, uidraw::VAlign::Top);
            }
            context.PushClip(plot);
            const uint32_t colors[] = {theme.colors.text, theme.colors.gizmoX, theme.colors.gizmoY, theme.colors.gizmoZ};
            const auto     toScreen = [&](float time, float value) {
                const float x = plot.left + (time - timeStart) / timeSpan * plot.Width();
                const float y = plot.bottom - (value - minimum) / std::max(1e-5f, maximum - minimum) * plot.Height();
                return sky::ui::UIPoint{x, y};
            };
            float previousTime  = timeStart;
            auto  previousValue = owner.curve.Evaluate(previousTime);
            for (int i = 1; i <= 96; ++i) {
                const float time  = timeStart + timeSpan * static_cast<float>(i) / 96.f;
                const auto  value = owner.curve.Evaluate(time);
                for (uint32_t channel = 0; channel < std::min(owner.curve.channels, 4u); ++channel) {
                    const auto previous = toScreen(previousTime, previousValue.v[channel]);
                    const auto next     = toScreen(time, value.v[channel]);
                    context.AddLine(previous.x, previous.y, next.x, next.y, theme.metrics.selectionWidth * .8f, colors[channel]);
                }
                previousTime  = time;
                previousValue = value;
            }
            const int primaryKey = owner.PrimarySelection();
            if (owner.curve.interpolation == curve::CurveInterpolation::CUBIC && primaryKey >= 0 &&
                primaryKey < static_cast<int>(owner.curve.keys.size())) {
                const auto &key      = owner.curve.keys[primaryKey];
                const float dt       = std::max(.02f, timeSpan * .12f);
                const auto  base     = toScreen(key.time, key.value.v[activeChannel]);
                const auto  outPoint = toScreen(key.time + dt, key.value.v[activeChannel] + key.tangentOut.v[activeChannel] * dt);
                const auto  inPoint  = toScreen(key.time - dt, key.value.v[activeChannel] - key.tangentIn.v[activeChannel] * dt);
                context.AddLine(base.x, base.y, outPoint.x, outPoint.y, theme.scale, colors[activeChannel]);
                context.AddLine(base.x, base.y, inPoint.x, inPoint.y, theme.scale, colors[activeChannel]);
                const float radius = theme.metrics.selectionMarkerRadius * .5f;
                context.AddRoundedRect({outPoint.x - radius, outPoint.y - radius, outPoint.x + radius, outPoint.y + radius},
                                       theme.colors.gizmoHighlight, radius);
                context.AddRoundedRect({inPoint.x - radius, inPoint.y - radius, inPoint.x + radius, inPoint.y + radius}, theme.colors.gizmoHighlight,
                                       radius);
            }
            for (int i = 0; i < static_cast<int>(owner.curve.keys.size()); ++i) {
                const auto &key = owner.curve.keys[i];
                if (key.time < timeStart || key.time > timeStart + timeSpan)
                    continue;
                const bool selected = std::find(owner.selection.begin(), owner.selection.end(), i) != owner.selection.end();
                for (uint32_t channel = 0; channel < std::min(owner.curve.channels, 4u); ++channel) {
                    const auto  point = toScreen(key.time, key.value.v[channel]);
                    const float size  = theme.metrics.selectionMarkerRadius * (selected && channel == activeChannel ? .75f : .5f);
                    context.AddRoundedRect({point.x - size, point.y - size, point.x + size, point.y + size},
                                           selected ? theme.colors.gizmoHighlight : colors[channel], size);
                }
            }
            if (mode == Mode::Marquee) {
                const sky::ui::UIRect box{std::min(marqueeStart.x, marqueeEnd.x), std::min(marqueeStart.y, marqueeEnd.y),
                                          std::max(marqueeStart.x, marqueeEnd.x), std::max(marqueeStart.y, marqueeEnd.y)};
                context.AddRect(box, theme.colors.selectionOccluded);
            }
            if (hasCursor && Contains(plot, cursorX, cursorY)) {
                const uint32_t guide = theme.colors.guide;
                context.AddRect({cursorX, plot.top, cursorX + theme.scale, plot.bottom}, guide);
                context.AddRect({plot.left, cursorY, plot.right, cursorY + theme.scale}, guide);
                if (text) {
                    const float time  = TimeAt(cursorX);
                    const float value = owner.curve.Evaluate(time).v[activeChannel];
                    char        readout[96];
                    std::snprintf(readout, sizeof(readout), "t %.*f   v %.*f", std::max(2, timeTicks.precision), time,
                                  std::max(2, valueTicks.precision), value);
                    const sky::ui::UIRect box{plot.right - theme.metrics.fieldWidth, plot.top + theme.metrics.checkboxPad,
                                              plot.right - theme.metrics.rowRadius, plot.top + theme.metrics.checkboxPad + theme.metrics.rowHeight};
                    context.AddRect(box, theme.colors.shadow);
                    uidraw::Text(context, readout, theme.fonts.label, box, theme.colors.text, text, uidraw::HAlign::Right, uidraw::VAlign::Middle);
                }
            }
            context.PopClip();
            if (text) {
                char status[128];
                std::snprintf(status, sizeof(status), "%u ch  %s  snap %s", owner.curve.channels,
                              owner.curve.interpolation == curve::CurveInterpolation::CUBIC  ? "cubic"
                              : owner.curve.interpolation == curve::CurveInterpolation::STEP ? "step"
                                                                                             : "linear",
                              owner.snap ? "on" : "off");
                uidraw::Text(context, status, theme.fonts.label, {plot.left, plot.top - theme.metrics.rowHeight, plot.right, plot.top},
                             theme.colors.textMuted, text, uidraw::HAlign::Left, uidraw::VAlign::Middle);
            }
        }

    private:
        enum class Mode { None, Pan, Keys, Marquee, TangentIn, TangentOut };
        struct Hit {
            int      key     = -1;
            uint32_t channel = 0;
        };
        AxisTicks MakeTicks(float minimum, float maximum, int target = 6) const
        {
            AxisTicks   ticks;
            const float span = maximum - minimum;
            if (!(span > 0.f) || !std::isfinite(span))
                return ticks;
            ticks.step        = NiceStep(span / static_cast<float>(target));
            ticks.precision   = ticks.step >= 1.f ? 0 : ticks.step >= .1f ? 1 : ticks.step >= .01f ? 2 : ticks.step >= .001f ? 3 : 4;
            const float first = std::ceil(minimum / ticks.step) * ticks.step;
            for (float value = first; value <= maximum + ticks.step * 1e-4f && ticks.values.size() < 32; value += ticks.step)
                ticks.values.push_back(value);
            return ticks;
        }
        sky::ui::UIRect Plot() const
        {
            const auto &theme = GetDefaultUiTheme();
            const float pad   = theme.metrics.controlPad;
            const auto  b     = GetBounds();
            float       minimum, maximum;
            ValueRange(minimum, maximum);
            char sample[32];
            std::snprintf(sample, sizeof(sample), "%.2f", maximum);
            const float labelWidth = std::max(theme.metrics.sliderValueWidth, uidraw::TextWidth(sample, theme.fonts.label, text) + pad * 2);
            return {b.left + pad + labelWidth, b.top + pad + theme.metrics.rowHeight, b.right - pad, b.bottom - pad - theme.metrics.rowHeight};
        }
        void ValueRange(float &minimum, float &maximum) const
        {
            if (mode == Mode::Keys || mode == Mode::TangentIn || mode == Mode::TangentOut) {
                minimum = dragMinimum;
                maximum = dragMaximum;
                return;
            }
            bool first = true;
            for (const auto &key : owner.curve.keys) {
                for (uint32_t c = 0; c < std::min(owner.curve.channels, 4u); ++c) {
                    const float value = key.value.v[c];
                    minimum           = first ? value : std::min(minimum, value);
                    maximum           = first ? value : std::max(maximum, value);
                    first             = false;
                }
            }
            if (first) {
                minimum = 0.f;
                maximum = 1.f;
            }
            if (maximum - minimum < 1e-4f) {
                minimum -= .5f;
                maximum += .5f;
            }
            const float padding = (maximum - minimum) * .08f;
            minimum -= padding;
            maximum += padding;
        }
        float TimeAt(float x) const
        {
            const auto &plot = Plot();
            if (plot.Width() <= 0.f)
                return 0.f;
            return std::clamp(timeStart + (x - plot.left) / plot.Width() * timeSpan, 0.f, 1.f);
        }
        float ValueAt(float y) const
        {
            const auto &plot = Plot();
            if (plot.Height() <= 0.f)
                return 0.f;
            float minimum, maximum;
            ValueRange(minimum, maximum);
            return maximum - (y - plot.top) / plot.Height() * (maximum - minimum);
        }
        float TimeStep() const
        {
            return NiceStep(timeSpan / 10.f);
        }
        float ValueStep() const
        {
            float minimum, maximum;
            ValueRange(minimum, maximum);
            return NiceStep((maximum - minimum) / 20.f);
        }
        void TrackCursor(float time)
        {
            owner.lastCursorTime = time;
            owner.hasCursorTime  = true;
        }
        void SetCursor(float x, float y)
        {
            cursorX   = x;
            cursorY   = y;
            hasCursor = true;
            MarkPaintDirty();
        }
        Hit HitKey(float x, float y) const
        {
            Hit        best;
            float      bestDistance = GetDefaultUiTheme().metrics.gizmoHitRadius;
            const auto plot         = Plot();
            float      minimum, maximum;
            ValueRange(minimum, maximum);
            for (int i = 0; i < static_cast<int>(owner.curve.keys.size()); ++i) {
                const auto &key = owner.curve.keys[i];
                if (key.time < timeStart || key.time > timeStart + timeSpan)
                    continue;
                for (uint32_t channel = 0; channel < std::min(owner.curve.channels, 4u); ++channel) {
                    const float px       = plot.left + (key.time - timeStart) / timeSpan * plot.Width();
                    const float py       = plot.bottom - (key.value.v[channel] - minimum) / std::max(1e-5f, maximum - minimum) * plot.Height();
                    const float distance = std::max(std::abs(px - x), std::abs(py - y));
                    if (distance <= bestDistance) {
                        bestDistance = distance;
                        best         = {i, channel};
                    }
                }
            }
            return best;
        }
        int HitTangent(int keyIndex, uint32_t channel, float x, float y) const
        {
            const auto &key  = owner.curve.keys[keyIndex];
            const auto  plot = Plot();
            float       minimum, maximum;
            ValueRange(minimum, maximum);
            const float dt    = std::max(.02f, timeSpan * .12f);
            const auto  point = [&](float time, float value) {
                return sky::ui::UIPoint{plot.left + (time - timeStart) / timeSpan * plot.Width(),
                                        plot.bottom - (value - minimum) / std::max(1e-5f, maximum - minimum) * plot.Height()};
            };
            const auto outPoint = point(key.time + dt, key.value.v[channel] + key.tangentOut.v[channel] * dt);
            const auto inPoint  = point(key.time - dt, key.value.v[channel] - key.tangentIn.v[channel] * dt);
            if (std::max(std::abs(outPoint.x - x), std::abs(outPoint.y - y)) <= GetDefaultUiTheme().metrics.gizmoHitRadius)
                return 1;
            if (std::max(std::abs(inPoint.x - x), std::abs(inPoint.y - y)) <= GetDefaultUiTheme().metrics.gizmoHitRadius)
                return -1;
            return 0;
        }
        void DragKeys(float x, float y)
        {
            float targetTime  = TimeAt(x);
            float targetValue = ValueAt(y);
            if (owner.snap) {
                const float ts = TimeStep(), vs = ValueStep();
                targetTime  = std::round(targetTime / ts) * ts;
                targetValue = std::round(targetValue / vs) * vs;
            }
            float dt    = targetTime - anchorTime;
            float dv    = targetValue - anchorValue;
            float lower = -std::numeric_limits<float>::infinity(), upper = std::numeric_limits<float>::infinity();
            for (const int index : dragSelection) {
                if (index < 0 || index >= static_cast<int>(dragStart.keys.size()) || index >= static_cast<int>(owner.curve.keys.size()))
                    continue;
                int prev = index - 1;
                while (prev >= 0 && std::find(dragSelection.begin(), dragSelection.end(), prev) != dragSelection.end())
                    --prev;
                int next = index + 1;
                while (next < static_cast<int>(dragStart.keys.size()) &&
                       std::find(dragSelection.begin(), dragSelection.end(), next) != dragSelection.end())
                    ++next;
                if (prev >= 0)
                    lower = std::max(lower, dragStart.keys[prev].time + 1e-4f - dragStart.keys[index].time);
                if (next < static_cast<int>(dragStart.keys.size()))
                    upper = std::min(upper, dragStart.keys[next].time - 1e-4f - dragStart.keys[index].time);
                lower = std::max(lower, -dragStart.keys[index].time);
                upper = std::min(upper, 1.f - dragStart.keys[index].time);
            }
            dt = lower <= upper ? std::clamp(dt, lower, upper) : 0.f;
            for (const int index : dragSelection) {
                if (index < 0 || index >= static_cast<int>(dragStart.keys.size()) || index >= static_cast<int>(owner.curve.keys.size()))
                    continue;
                auto &key                  = owner.curve.keys[index];
                key.time                   = std::clamp(dragStart.keys[index].time + dt, 0.f, 1.f);
                key.value.v[activeChannel] = std::clamp(dragStart.keys[index].value.v[activeChannel] + dv, -1e6f, 1e6f);
            }
            MarkPaintDirty();
            owner.MarkPaintDirty();
            if (owner.list)
                owner.list->Refresh();
        }
        void DragTangent(float x, float y)
        {
            if (tangentKey < 0 || tangentKey >= static_cast<int>(owner.curve.keys.size()))
                return;
            auto       &key   = owner.curve.keys[tangentKey];
            const float dt    = std::max(.02f, timeSpan * .12f);
            const float value = ValueAt(y);
            if (mode == Mode::TangentOut)
                key.tangentOut.v[activeChannel] = (value - key.value.v[activeChannel]) / dt;
            else
                key.tangentIn.v[activeChannel] = (key.value.v[activeChannel] - value) / dt;
            MarkPaintDirty();
            owner.MarkPaintDirty();
        }
        void SelectInMarquee()
        {
            const sky::ui::UIRect box{std::min(marqueeStart.x, marqueeEnd.x), std::min(marqueeStart.y, marqueeEnd.y),
                                      std::max(marqueeStart.x, marqueeEnd.x), std::max(marqueeStart.y, marqueeEnd.y)};
            if (box.Width() < GetDefaultUiTheme().metrics.viewportClickDistance && box.Height() < GetDefaultUiTheme().metrics.viewportClickDistance) {
                owner.SetSelection({}, -1);
                return;
            }
            const auto plot = Plot();
            float      minimum, maximum;
            ValueRange(minimum, maximum);
            std::vector<int> selected;
            for (int i = 0; i < static_cast<int>(owner.curve.keys.size()); ++i) {
                const auto &key = owner.curve.keys[i];
                for (uint32_t channel = 0; channel < std::min(owner.curve.channels, 4u); ++channel) {
                    const float px = plot.left + (key.time - timeStart) / timeSpan * plot.Width();
                    const float py = plot.bottom - (key.value.v[channel] - minimum) / std::max(1e-5f, maximum - minimum) * plot.Height();
                    if (Contains(box, px, py)) {
                        selected.push_back(i);
                        break;
                    }
                }
            }
            const int primaryIndex = selected.empty() ? -1 : selected.back();
            owner.SetSelection(std::move(selected), primaryIndex);
        }

        CurveEditorWidget     &owner;
        sky::ui::UITextSystem *text          = nullptr;
        float                  timeStart     = 0.f;
        float                  timeSpan      = 1.f;
        float                  lastX         = 0.f;
        uint32_t               activeChannel = 0;
        Mode                   mode          = Mode::None;
        uint32_t               pointer       = 0;
        curve::Curve           dragStart;
        std::vector<int>       dragSelection;
        int                    primary     = -1;
        int                    tangentKey  = -1;
        float                  anchorTime  = 0.f;
        float                  anchorValue = 0.f;
        float                  dragMinimum = 0.f, dragMaximum = 1.f;
        sky::ui::UIPoint       marqueeStart{}, marqueeEnd{};
        float                  cursorX = 0.f, cursorY = 0.f;
        bool                   hasCursor = false;
    };

    CurveEditorWidget::CurveEditorWidget(sky::ui::UITextSystem *inText, CommandService &inCommands, CurveEditorCallbacks inCallbacks)
        : callbacks(std::move(inCallbacks)), commands(inCommands), text(inText)
    {
        const ReflectedFormContext context{commands, EditorCore::GetPropertyEditors(), EditorCore::GetPropertyChanges(), GetDefaultUiTheme()};
        auto                       graph = std::make_unique<Graph>(*this);
        graph->SetText(text);
        this->graph = static_cast<Graph *>(AddChild(std::move(graph)));
        auto list   = std::make_unique<KeyList>(*this);
        this->list  = static_cast<KeyList *>(AddChild(std::move(list)));
        form        = static_cast<ReflectedFormView *>(AddChild(std::make_unique<ReflectedFormView>(text, "Selected Curve Object", context)));
        form->Form().SetEditCommandFactory([this](const PropertyDescriptor &field, Any value) {
            if (!field.SetValue(value))
                return UndoCommandPtr{};
            auto command = callbacks.makeEdit ? callbacks.makeEdit("Edit Curve property", curve) : UndoCommandPtr{};
            // The form executes this command once; later external edits must remain detectable.
            if (command)
                propertyEditRevision = commands.GetRevision() + 1;
            if (!command) {
                if (callbacks.current)
                    curve = callbacks.current();
                syncPending = true;
                MarkPaintDirty();
            }
            return command;
        });
        form->SetOnEdited([this] {
            modelRevision = propertyEditRevision;
            syncPending   = true;
            MarkPaintDirty();
        });
        beforeHistory = commands.AddBeforeHistoryCallback([this] { Cancel(); });
        modelRevision = commands.GetRevision();
        BindSelection();
    }
    CurveEditorWidget::~CurveEditorWidget()
    {
        commands.RemoveBeforeHistoryCallback(beforeHistory);
        Cancel();
        ClearChildren();
    }
    void CurveEditorWidget::SetCurve(const curve::Curve &value)
    {
        Cancel();
        curve         = value;
        modelRevision = commands.GetRevision();
        syncPending   = false;
        BindSelection();
    }
    const curve::Curve &CurveEditorWidget::GetCurve() const
    {
        return curve;
    }
    const std::vector<int> &CurveEditorWidget::Selection() const
    {
        return selection;
    }
    int CurveEditorWidget::PrimarySelection() const
    {
        return primarySelection;
    }
    void CurveEditorWidget::SetSelection(std::vector<int> indices, int primary)
    {
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        indices.erase(
            std::remove_if(indices.begin(), indices.end(), [&](int index) { return index < 0 || index >= static_cast<int>(curve.keys.size()); }),
            indices.end());
        selection        = std::move(indices);
        primarySelection = std::binary_search(selection.begin(), selection.end(), primary) ? primary : -1;
        BindSelection();
    }
    void CurveEditorWidget::BindSelection()
    {
        if (!form)
            return;
        form->CancelInteractions();
        selection.erase(
            std::remove_if(selection.begin(), selection.end(), [&](int index) { return index < 0 || index >= static_cast<int>(curve.keys.size()); }),
            selection.end());
        if (!std::binary_search(selection.begin(), selection.end(), primarySelection))
            primarySelection = selection.empty() ? -1 : selection.back();
        const int            primary = PrimarySelection();
        const PropertyObject object = primary >= 0 ? PropertyObject{&curve.keys[primary], sky::GetTypeNode(TypeInfo<curve::CurveKey>::RegisteredId())}
                                                   : PropertyObject{&curve, sky::GetTypeNode(TypeInfo<curve::Curve>::RegisteredId())};
        form->Bind(object);
        if (list)
            list->Refresh();
        if (graph)
            graph->SynchronizeModel();
        MarkPaintDirty();
    }
    void CurveEditorWidget::Commit(std::string label, curve::Curve next)
    {
        auto command = callbacks.makeEdit ? callbacks.makeEdit(std::move(label), std::move(next)) : UndoCommandPtr{};
        if (!command || !commands.Execute(std::move(command))) {
            if (callbacks.current)
                SetCurve(callbacks.current());
            return;
        }
        if (callbacks.current)
            SetCurve(callbacks.current());
    }
    void CurveEditorWidget::AddKeyAt(float time)
    {
        if (curve.keys.size() >= curve::MAX_CURVE_KEYS)
            return;
        time = std::clamp(time, 0.f, 1.f);
        for (const auto &key : curve.keys)
            if (std::abs(key.time - time) < 1e-4f)
                return;
        auto            next = curve;
        curve::CurveKey key;
        key.time  = time;
        key.value = curve.Evaluate(time);
        next.keys.push_back(key);
        std::sort(next.keys.begin(), next.keys.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
        selection.clear();
        for (int i = 0; i < static_cast<int>(next.keys.size()); ++i)
            if (next.keys[i].time == time)
                selection = {i};
        Commit("Add Curve key", std::move(next));
    }
    void CurveEditorWidget::RemoveSelected()
    {
        if (selection.empty() || selection.size() >= curve.keys.size())
            return;
        auto next = curve;
        for (auto it = selection.rbegin(); it != selection.rend(); ++it)
            next.keys.erase(next.keys.begin() + *it);
        selection.clear();
        Commit("Remove Curve keys", std::move(next));
    }
    void CurveEditorWidget::CopySelected()
    {
        if (selection.empty())
            return;
        clipboard.clear();
        for (const int index : selection)
            if (index >= 0 && index < static_cast<int>(curve.keys.size()))
                clipboard.push_back(curve.keys[index]);
    }
    void CurveEditorWidget::Paste(bool duplicate)
    {
        std::vector<curve::CurveKey> source;
        if (duplicate) {
            for (const int index : selection)
                if (index >= 0 && index < static_cast<int>(curve.keys.size()))
                    source.push_back(curve.keys[index]);
        } else {
            source = clipboard;
        }
        if (source.empty())
            return;
        float target   = hasCursorTime ? lastCursorTime : (selection.empty() ? .5f : curve.keys[PrimarySelection()].time + .05f);
        float earliest = source.front().time;
        for (const auto &key : source)
            earliest = std::min(earliest, key.time);
        const float shift = target - earliest;
        auto        next  = curve;
        for (auto key : source) {
            key.time = std::clamp(key.time + shift, 0.f, 1.f);
            next.keys.push_back(key);
        }
        std::sort(next.keys.begin(), next.keys.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
        std::string error;
        if (!next.Validate(error))
            return;
        selection.clear();
        for (int i = 0; i < static_cast<int>(next.keys.size()); ++i) {
            for (const auto &key : source) {
                const float time = std::clamp(key.time + shift, 0.f, 1.f);
                if (next.keys[i].time == time && next.keys[i].value.x == key.value.x && next.keys[i].value.y == key.value.y)
                    selection.push_back(i);
            }
        }
        std::sort(selection.begin(), selection.end());
        selection.erase(std::unique(selection.begin(), selection.end()), selection.end());
        Commit(duplicate ? "Duplicate Curve keys" : "Paste Curve keys", std::move(next));
    }
    void CurveEditorWidget::SetInterpolation(curve::CurveInterpolation value)
    {
        if (curve.interpolation == value)
            return;
        auto next          = curve;
        next.interpolation = value;
        Commit("Set Curve interpolation", std::move(next));
    }
    bool CurveEditorWidget::CanExecute(CurveEditorCommand command) const
    {
        if (Editing())
            return false;
        switch (command) {
        case CurveEditorCommand::AddKey: return curve.keys.size() < curve::MAX_CURVE_KEYS;
        case CurveEditorCommand::RemoveSelected: return !selection.empty() && selection.size() < curve.keys.size();
        case CurveEditorCommand::Duplicate:
        case CurveEditorCommand::Copy: return !selection.empty();
        case CurveEditorCommand::Paste: return !clipboard.empty();
        case CurveEditorCommand::FrameAll:
        case CurveEditorCommand::ToggleSnap:
        case CurveEditorCommand::SetLinear:
        case CurveEditorCommand::SetStep:
        case CurveEditorCommand::SetCubic: return true;
        default: return false;
        }
    }
    void CurveEditorWidget::Execute(CurveEditorCommand command)
    {
        SynchronizeCurrent();
        Cancel();
        if (!CanExecute(command))
            return;
        switch (command) {
        case CurveEditorCommand::AddKey: AddKeyAt(hasCursorTime ? lastCursorTime : .5f); break;
        case CurveEditorCommand::RemoveSelected: RemoveSelected(); break;
        case CurveEditorCommand::Duplicate: Paste(true); break;
        case CurveEditorCommand::Copy: CopySelected(); break;
        case CurveEditorCommand::Paste: Paste(false); break;
        case CurveEditorCommand::FrameAll:
            if (graph)
                graph->FrameAll();
            break;
        case CurveEditorCommand::ToggleSnap:
            snap = !snap;
            if (graph)
                graph->MarkPaintDirty();
            break;
        case CurveEditorCommand::SetLinear: SetInterpolation(curve::CurveInterpolation::LINEAR); break;
        case CurveEditorCommand::SetStep: SetInterpolation(curve::CurveInterpolation::STEP); break;
        case CurveEditorCommand::SetCubic: SetInterpolation(curve::CurveInterpolation::CUBIC); break;
        default: break;
        }
    }
    void CurveEditorWidget::Tick(float)
    {
        SynchronizeCurrent();
        if (syncPending && !Editing()) {
            syncPending = false;
            if (callbacks.current)
                curve = callbacks.current();
            BindSelection();
        }
    }
    void CurveEditorWidget::SynchronizeCurrent()
    {
        if (modelRevision != commands.GetRevision() && callbacks.current)
            SetCurve(callbacks.current());
    }
    bool CurveEditorWidget::Editing() const
    {
        return (form && form->HasActiveInteraction()) || (graph && graph->Editing());
    }
    bool CurveEditorWidget::AcceptsCommandShortcuts() const
    {
        return !form || !form->HasActiveInteraction();
    }
    bool CurveEditorWidget::SnapEnabled() const
    {
        return snap;
    }
    void CurveEditorWidget::Cancel()
    {
        if (form)
            form->CancelInteractions();
        if (graph)
            graph->Cancel();
    }
    void CurveEditorWidget::OnPaint(sky::ui::UIPaintContext &context)
    {
        context.AddRect(GetBounds(), GetDefaultUiTheme().colors.panel);
    }
    void CurveEditorWidget::ArrangeChildren(const sky::ui::UIRect &content)
    {
        if (!graph || !form)
            return;
        const auto &theme = GetDefaultUiTheme();
        const float pad   = theme.metrics.controlPad;
        const auto  inner = sky::ui::UIRect{content.left + pad, content.top + pad, content.right - pad, content.bottom - pad};
        const float split = inner.top + inner.Height() * .58f;
        graph->Arrange({inner.left, inner.top, inner.right, split});
        const float listWidth = std::min(inner.Width(), std::max(theme.metrics.listColumnWidth, inner.Width() * .35f));
        if (list) {
            list->SetItemHeight(theme.metrics.rowHeight);
            list->Arrange({inner.left, split + pad, inner.left + listWidth, inner.bottom});
        }
        form->Arrange({inner.left + listWidth + pad, split + pad, inner.right, inner.bottom});
    }
    sky::ui::UIEventResult CurveEditorWidget::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        SynchronizeCurrent();
        if (graph && graph->WantsPointerCapture())
            return graph->OnPointerEvent(event);
        if (form && (form->WantsPointerCapture() || form->GetBounds().Contains(event.x, event.y))) {
            const auto result = form->OnPointerEvent(event);
            if (result == sky::ui::UIEventResult::HANDLED || form->WantsPointerCapture())
                return result;
        }
        if (graph && graph->GetBounds().Contains(event.x, event.y))
            return graph->OnPointerEvent(event);
        if (list && list->GetBounds().Contains(event.x, event.y))
            return list->OnPointerEvent(event);
        return sky::ui::UIEventResult::UNHANDLED;
    }
    bool CurveEditorWidget::WantsPointerCapture() const
    {
        return (form && form->WantsPointerCapture()) || (graph && graph->WantsPointerCapture());
    }
    sky::ui::UIEventResult CurveEditorWidget::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        SynchronizeCurrent();
        if (form && form->OnKeyEvent(event) == sky::ui::UIEventResult::HANDLED)
            return sky::ui::UIEventResult::HANDLED;
        if (event.action != sky::ui::UIKeyAction::DOWN && event.action != sky::ui::UIKeyAction::REPEAT)
            return sky::ui::UIEventResult::UNHANDLED;
        if (event.keyCode == 27) {
            Cancel();
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }
    sky::ui::UIEventResult CurveEditorWidget::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        SynchronizeCurrent();
        return form && form->OnTextInput(event) == sky::ui::UIEventResult::HANDLED ? sky::ui::UIEventResult::HANDLED
                                                                                   : sky::ui::UIEventResult::UNHANDLED;
    }
    void CurveEditorWidget::OnPointerCancel(uint32_t pointer)
    {
        if (form)
            form->OnPointerCancel(pointer);
        if (graph)
            graph->Cancel();
    }
    void CurveEditorWidget::OnPointerButtonConflict(uint32_t pointer)
    {
        OnPointerCancel(pointer);
    }
    void CurveEditorWidget::OnPointerLeave(const sky::ui::UIPointerEvent &event)
    {
        if (form)
            form->OnPointerLeave(event);
        if (graph)
            graph->OnPointerLeave(event);
    }
} // namespace sky::editor
