//
// Created by blues on 2024/5/19.
//

#include <editor/widgets/InspectorWidget.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <framework/serialization/SerializationUtil.h>

namespace sky::editor {

    InspectorWidget::InspectorWidget() : ImWidget("Inspector")
    {
        SelectEvent::Connect(this);
    }

    InspectorWidget::~InspectorWidget()
    {
        SelectEvent::DisConnect(this);
    }

    void InspectorWidget::Execute(ImContext &context)
    {
        if (!show) {
            return;
        }

        if (ImGui::Begin("Details")) {
            ShowDetails();
            ImGui::End();
        }
    }

    void InspectorWidget::ShowDetails()
    {
        if (actor == nullptr) {
            return;
        }

        Uuid delId;
        const auto &components = actor->GetComponents();
        for (const auto &[id, component] : components) {
            ImGui::Separator();
            ImGui::BeginGroup();
            if (ShowComponent(id, component.get())) {
                delId = id;
            }
            ImGui::EndGroup();
        }
        if (static_cast<bool>(delId)) {
            actor->RemoveComponent(delId);
        }

//        if (ImGui::BeginPopupContextItem(0, ImGuiPopupFlags_AnyPopup)) {
//            if (ImGui::MenuItem("Delete")) {}
//            ImGui::EndPopup();
//        }
    }

    bool InspectorWidget::ShowComponent(const Uuid &id, ComponentBase *comp)
    {
        const auto *node = GetTypeNode(id);
        ImGui::Text("%s", node->info->name.data());
        ImGui::SameLine();
        if (ImGui::Button("x")) {
            return true;
        }

        for (const auto &member : node->members) {
            auto data = GetValueRawConst(comp, id, member.first.data());
            if (member.second.info->staticInfo->isEnum) {
                auto *v = data.GetAs<uint64_t>();
                const auto *enumInfo = GetTypeNode(member.second.info->registeredId);
                auto enumText = enumInfo->enums.at(*v);
                if (ImGui::BeginCombo(member.first.data(), enumText.data())) {
                    for (const auto &[val, key] : enumInfo->enums) {
                        bool isSelect = false;
                        ImGui::Selectable(key.data(), &isSelect);

                        if (isSelect) {
                            SetValueRaw(comp, id, member.first.data(), &val);
                        }
                    }
                    ImGui::EndCombo();
                }
            } else {
                const char *fFormat = "%.3f";
                const auto fRules = ImGuiInputTextFlags_EnterReturnsTrue;
                float step = 0.05f;
                float stepFast = 0.1f;

                if (member.second.info->registeredId == TypeInfo<float>::RegisteredId()) {
                    auto *v = reinterpret_cast<float *>(data.GetAs<float>());
                    if (ImGui::InputScalarN(member.first.data(), ImGuiDataType_Float, v, 1, &step, &stepFast, fFormat, fRules)) {
                        SetValueRaw(comp, id, member.first.data(), v);
                    }
                }
                else if (member.second.info->registeredId == TypeInfo<Vector2>::RegisteredId()) {
                    auto *v = reinterpret_cast<float *>(data.GetAs<Vector2>());
                    if (ImGui::InputScalarN(member.first.data(), ImGuiDataType_Float, v, 2, &step, &stepFast, fFormat, fRules)) {
                        SetValueRaw(comp, id, member.first.data(), v);
                    }
                }
                else if (member.second.info->registeredId == TypeInfo<Vector3>::RegisteredId()) {
                    auto *v = reinterpret_cast<float *>(data.GetAs<Vector3>());
                    if (ImGui::InputScalarN(member.first.data(), ImGuiDataType_Float, v, 3, &step, &stepFast, fFormat, fRules)) {
                        SetValueRaw(comp, id, member.first.data(), v);
                    }
                }
                else if (member.second.info->registeredId == TypeInfo<Vector4>::RegisteredId()) {
                    auto *v = reinterpret_cast<float *>(data.GetAs<Vector4>());
                    if (ImGui::InputScalarN(member.first.data(), ImGuiDataType_Float, v, 4, &step, &stepFast, fFormat, fRules)) {
                        SetValueRaw(comp, id, member.first.data(), v);
                    }
                }
                else if (member.second.info->registeredId == TypeInfo<bool>::RegisteredId()) {
                    auto *v = reinterpret_cast<bool *>(data.GetAs<bool>());
                    if (ImGui::Checkbox(member.first.data(), v)) {
                        SetValueRaw(comp, id, member.first.data(), v);
                    }
                }
                else if (member.second.info->registeredId == TypeInfo<Uuid>::RegisteredId()) {
                    auto *v = reinterpret_cast<Uuid *>(data.GetAs<Uuid>());
                    auto uuid = v->ToString();
                    if (ImGui::InputText(member.first.data(), uuid.data(), uuid.length() + 1)) {
                        auto assetId = Uuid::CreateFromString(uuid);
                        SetValueRaw(comp, id, member.first.data(), &assetId);
                    }
                }
            }
        }
        return false;
    }

    void InspectorWidget::BindEvent(EventID id)
    {
        binder.Bind(this, id);
    }

} // namespace sky::editor