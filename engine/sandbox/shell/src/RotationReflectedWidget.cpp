//
// Created on 2026/10/04.
//

#include <editor/shell/RotationReflectedWidget.h>
#include <editor/shell/UiDraw.h>

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
            Any copy = field.descriptor.GetValue();
            const serialize::TypeMemberNode *member = FloatMemberAt(field.descriptor.GetStructType(), ordinal);
            if (copy.Data() == nullptr || member == nullptr) {
                return 0.0f;
            }
            PropertyDescriptor child(copy.Data(), member, std::string(), field.descriptor.GetCategory());
            const Any value = child.GetValue();
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
            Any copy = field.descriptor.GetValue();
            const TypeNode *node = field.descriptor.GetStructType();
            if (copy.Data() == nullptr || node == nullptr) {
                return {};
            }
            const char *names[4] = {"x", "y", "z", "w"};
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

    sky::ui::UIRect RotationReflectedWidget::Cell(const sky::ui::UIRect &control, int index, int count) const
    {
        const float span = (control.right - control.left) / static_cast<float>(std::max(count, 1));
        return sky::ui::UIRect{control.left + span * static_cast<float>(index) + 1.0f, control.top,
                               control.left + span * static_cast<float>(index + 1) - 2.0f, control.bottom};
    }

    void RotationReflectedWidget::Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context,
                                        PropertyField &field, const sky::ui::UIRect &rect)
    {
        const UiTheme &th = host.Skin().Theme();
        const sky::ui::UIRect body{rect.left, rect.top, rect.right - 44.0f, rect.bottom};
        const sky::ui::UIRect button{rect.right - 42.0f, rect.top, rect.right, rect.bottom};

        const Quaternion q = ReadQuaternion(field);
        if (mode == 0) {
            const Vector3 euler = q.ToEulerYZX();
            const float values[3] = {euler.x, euler.y, euler.z};
            static const char *labels[3] = {"X", "Y", "Z"};
            for (int i = 0; i < 3; ++i) {
                const sky::ui::UIRect cell = Cell(body, i, 3);
                host.Skin().DrawField(context, cell, false, false);
                uc::Text(context, labels[i], th.fonts.tiny,
                         sky::ui::UIRect{cell.left + 4.0f, cell.top, cell.left + 15.0f, cell.bottom}, th.colors.textMuted, host.Text());
                char buffer[32] = {0};
                std::snprintf(buffer, sizeof(buffer), "%.1f", values[i]);
                uc::Text(context, buffer, th.fonts.small,
                         sky::ui::UIRect{cell.left + 15.0f, cell.top, cell.right - 3.0f, cell.bottom}, th.colors.text, host.Text());
            }
        } else {
            const float values[4] = {q.x, q.y, q.z, q.w};
            static const char *labels[4] = {"x", "y", "z", "w"};
            for (int i = 0; i < 4; ++i) {
                const sky::ui::UIRect cell = Cell(body, i, 4);
                host.Skin().DrawField(context, cell, false, false);
                uc::Text(context, labels[i], th.fonts.tiny,
                         sky::ui::UIRect{cell.left + 4.0f, cell.top, cell.left + 14.0f, cell.bottom}, th.colors.textMuted, host.Text());
                char buffer[32] = {0};
                std::snprintf(buffer, sizeof(buffer), "%.2f", values[i]);
                uc::Text(context, buffer, th.fonts.small,
                         sky::ui::UIRect{cell.left + 14.0f, cell.top, cell.right - 3.0f, cell.bottom}, th.colors.text, host.Text());
            }
        }

        host.Skin().DrawField(context, button, false, false);
        uc::Text(context, mode == 0 ? "YZX" : "Quat", th.fonts.tiny, button, th.colors.textMuted, host.Text(), uc::HAlign::Center);
    }

    bool RotationReflectedWidget::OnDown(ReflectedWidgetHost &host, PropertyField &field,
                                         const sky::ui::UIPointerEvent &event, const sky::ui::UIRect &rect)
    {
        if (event.x >= rect.right - 44.0f) {
            mode = (mode + 1) % 2;
            host.MarkDirty();
            return true;
        }

        const sky::ui::UIRect body{rect.left, rect.top, rect.right - 44.0f, rect.bottom};
        const int count = (mode == 0) ? 3 : 4;
        for (int i = 0; i < count; ++i) {
            if (!Cell(body, i, count).Contains(event.x, rect.top)) {
                continue;
            }
            ReflectedWidgetHost *h = &host;
            PropertyField *t = &field;
            const int component = i;
            const int currentMode = mode;
            char initial[32] = {0};
            if (currentMode == 0) {
                std::snprintf(initial, sizeof(initial), "%.2f", ReadQuaternion(field).ToEulerYZX().v[i]);
            } else {
                const Quaternion q = ReadQuaternion(field);
                std::snprintf(initial, sizeof(initial), "%.3f", q.v[i]);
            }
            host.BeginTextEdit(field, component, PropertyEditorKind::Float, initial, [h, t, component, currentMode](const std::string &text) {
                char *end = nullptr;
                const float value = std::strtof(text.c_str(), &end);
                if (end == text.c_str()) {
                    return false;
                }
                Quaternion q = ReadQuaternion(*t);
                if (currentMode == 0) {
                    Vector3 euler = q.ToEulerYZX();
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
