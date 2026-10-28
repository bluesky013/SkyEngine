//
// Created on 2026/10/04.
//

#include <editor/widgets/RotationReflectedWidget.h>

#include <editor/ui/UiDraw.h>

#include <editor/core/command/CommandService.h>

#include <core/math/Quaternion.h>
#include <core/math/Vector3.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

        const serialize::TypeMemberNode *FloatMemberAt(const TypeNode *node, int ordinal)
        {
            if (node == nullptr) {
                return nullptr;
            }
            int idx = 0;
            for (const auto &entry : node->members) {
                const TypeInfoRT *info = entry.second.info;
                if (info == nullptr || info->staticInfo == nullptr || !info->staticInfo->isFloatingPoint) {
                    continue;
                }
                if (idx == ordinal) {
                    return &entry.second;
                }
                ++idx;
            }
            return nullptr;
        }

        bool FloatMemberIndex(const PropertyDescriptor &descriptor, const char *name, int &outIndex)
        {
            const TypeNode *node = descriptor.GetStructType();
            if (node == nullptr) {
                return false;
            }
            int idx = 0;
            for (const auto &entry : node->members) {
                const TypeInfoRT *info = entry.second.info;
                if (info == nullptr || info->staticInfo == nullptr || !info->staticInfo->isFloatingPoint) {
                    continue;
                }
                std::string memberName(entry.first);
                for (char &ch : memberName) {
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                }
                if (memberName == name) {
                    outIndex = idx;
                    return true;
                }
                ++idx;
            }
            return false;
        }

        float ReadChannel(const PropertyField &field, const char *name)
        {
            int ordinal = -1;
            if (!FloatMemberIndex(field.descriptor, name, ordinal)) {
                return 0.0f;
            }
            Any                              copy   = field.descriptor.GetValue();
            const serialize::TypeMemberNode *member = FloatMemberAt(field.descriptor.GetStructType(), ordinal);
            if (copy.Data() == nullptr || member == nullptr) {
                return 0.0f;
            }
            PropertyDescriptor child(copy.Data(), member, std::string(), field.descriptor.GetCategory());
            const Any          value = child.GetValue();
            if (const float *f = value.GetAsConst<float>()) {
                return *f;
            }
            return 0.0f;
        }

        Quaternion ReadQuaternion(const PropertyField &field)
        {
            Quaternion q;
            q.x = ReadChannel(field, "x");
            q.y = ReadChannel(field, "y");
            q.z = ReadChannel(field, "z");
            q.w = ReadChannel(field, "w");
            return q;
        }

        Any BuildQuaternionValue(const PropertyField &field, const Quaternion &quat)
        {
            Any             copy = field.descriptor.GetValue();
            const TypeNode *node = field.descriptor.GetStructType();
            if (copy.Data() == nullptr || node == nullptr) {
                return {};
            }
            const char *names[4]  = {"x", "y", "z", "w"};
            const float values[4] = {quat.x, quat.y, quat.z, quat.w};
            for (int c = 0; c < 4; ++c) {
                int ordinal = -1;
                if (!FloatMemberIndex(field.descriptor, names[c], ordinal)) {
                    continue;
                }
                const serialize::TypeMemberNode *member = FloatMemberAt(node, ordinal);
                if (member == nullptr) {
                    continue;
                }
                PropertyDescriptor child(copy.Data(), member, std::string(), field.descriptor.GetCategory());
                child.SetValue(Any(values[c]));
            }
            return copy;
        }

    } // namespace

    RotationReflectedWidget::Layout RotationReflectedWidget::ComputeLayout(const ReflectedWidgetHost &host, const sky::ui::UIRect &control) const
    {
        const UiTheme &theme = host.Skin().Theme();
        const float    buttonWidth =
            std::max(uc::TextWidth("YZX", theme.fonts.tiny, host.Text()), uc::TextWidth("Quat", theme.fonts.tiny, host.Text())) +
            2.0f * theme.metrics.controlPad;
        const float buttonLeft = std::max(control.left, control.right - buttonWidth);
        return {{control.left, control.top, std::max(control.left, buttonLeft - theme.metrics.itemSpacing), control.bottom},
                {buttonLeft, control.top, control.right, control.bottom}};
    }

    sky::ui::UIRect RotationReflectedWidget::Cell(const UiMetrics &metrics, const sky::ui::UIRect &control, int index, int count) const
    {
        count             = std::max(count, 1);
        const float gap   = std::min(metrics.itemSpacing, std::max(0.0f, control.Width()) / static_cast<float>(count));
        const float width = std::max(0.0f, (control.Width() - gap * static_cast<float>(count - 1)) / static_cast<float>(count));
        const float left  = control.left + (width + gap) * static_cast<float>(index);
        return {left, control.top, left + width, control.bottom};
    }

    void
    RotationReflectedWidget::Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect)
    {
        const UiTheme &th     = host.Skin().Theme();
        const Layout   layout = ComputeLayout(host, rect);

        const Quaternion   q            = ReadQuaternion(field);
        const Vector3      euler        = q.ToEulerYZX();
        const int          count        = mode == 0 ? 3 : 4;
        static const char *labels[2][4] = {{"X", "Y", "Z", ""}, {"x", "y", "z", "w"}};
        for (int i = 0; i < count; ++i) {
            const sky::ui::UIRect cell = Cell(th.metrics, layout.body, i, count);
            host.Skin().DrawField(context, cell, false, false);
            const float labelLeft  = std::min(cell.right, cell.left + th.metrics.controlPad);
            const float labelRight = std::min(cell.right, labelLeft + uc::TextWidth(labels[mode][i], th.fonts.tiny, host.Text()));
            uc::Text(context, labels[mode][i], th.fonts.tiny, {labelLeft, cell.top, labelRight, cell.bottom}, th.colors.textMuted, host.Text(),
                     uc::HAlign::Left, uc::VAlign::Middle, false);
            char buffer[32] = {0};
            std::snprintf(buffer, sizeof(buffer), mode == 0 ? "%.1f" : "%.2f", mode == 0 ? euler.v[i] : q.v[i]);
            const float valueLeft = std::min(cell.right, labelRight + th.metrics.controlPad);
            uc::Text(context, buffer, th.fonts.small,
                     sky::ui::UIRect{valueLeft, cell.top, std::max(valueLeft, cell.right - th.metrics.controlPad), cell.bottom}, th.colors.text,
                     host.Text());
        }

        host.Skin().DrawField(context, layout.button, false, false);
        uc::Text(context, mode == 0 ? "YZX" : "Quat", th.fonts.tiny, layout.button, th.colors.textMuted, host.Text(), uc::HAlign::Center);
    }

    bool RotationReflectedWidget::OnDown(ReflectedWidgetHost           &host,
                                         PropertyField                 &field,
                                         const sky::ui::UIPointerEvent &event,
                                         const sky::ui::UIRect         &rect)
    {
        const Layout layout = ComputeLayout(host, rect);
        if (layout.button.Contains(event.x, event.y)) {
            mode = (mode + 1) % 2;
            host.MarkDirty();
            return true;
        }

        const int count = (mode == 0) ? 3 : 4;
        for (int i = 0; i < count; ++i) {
            if (!Cell(host.Skin().Theme().metrics, layout.body, i, count).Contains(event.x, event.y)) {
                continue;
            }
            ReflectedWidgetHost *h           = &host;
            PropertyField       *t           = &field;
            const int            component   = i;
            const int            currentMode = mode;
            char                 initial[32] = {0};
            if (currentMode == 0) {
                std::snprintf(initial, sizeof(initial), "%.2f", ReadQuaternion(field).ToEulerYZX().v[i]);
            } else {
                const Quaternion q = ReadQuaternion(field);
                std::snprintf(initial, sizeof(initial), "%.3f", q.v[i]);
            }
            host.BeginTextEdit(field, component, PropertyEditorKind::Float, initial, [h, t, component, currentMode](const std::string &text) {
                char       *end   = nullptr;
                const float value = std::strtof(text.c_str(), &end);
                if (end == text.c_str()) {
                    return false;
                }
                Quaternion q = ReadQuaternion(*t);
                if (currentMode == 0) {
                    Vector3 euler      = q.ToEulerYZX();
                    euler.v[component] = value;
                    q.FromEulerYZX(euler);
                } else {
                    q.v[component] = value;
                }
                h->Form().Edit(*t, BuildQuaternionValue(*t, q), h->Commands());
                h->RefreshForm();
                return true;
            });
            return true;
        }
        return false;
    }

} // namespace sky::editor
