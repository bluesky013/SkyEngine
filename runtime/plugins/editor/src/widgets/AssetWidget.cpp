//
// Created by blues on 2024/3/31.
//

#include <editor/widgets/AssetWidget.h>
#include <framework/asset/AssetManager.h>
#include <filesystem>

namespace sky::editor {

    void AssetWidget::Execute(ImContext &context)
    {
        if (!show) {
            selectedId = Uuid();
            return;
        }

//        auto *am = AssetManager::Get();
//        const auto &package = am->GetIDMap();
//        if (ImGui::Begin(name.c_str())) {
//
//            ImGui::Text("Asset List. items(%u)", static_cast<uint32_t>(package.size()));
//
//            ImGui::InputText("Search", inputText.data(), MAX_INPUT_LENGTH);
//            if (ImGui::BeginListBox("assets", ImVec2(-FLT_MIN, 16 * ImGui::GetTextLineHeightWithSpacing()))) {
//                for (const auto &[key, info]: package) {
//                    bool clicked = false;
//                    auto fileName = std::filesystem::path(info.loadPath).filename().string();
//
//                    std::string_view view(inputText.data());
//                    if (fileName.find(view) == std::string::npos) {
//                        continue;
//                    }
//
//                    if(ImGui::Selectable(fileName.c_str(), &clicked, ImGuiSelectableFlags_AllowDoubleClick)) {
//                        selectedId = key;
//                    }
//                }
//                ImGui::EndListBox();
//            }
//
//            ImGui::Separator();
//
//            if (static_cast<bool>(selectedId)) {
//                ImGui::BulletText("UUID: %s", selectedId.ToString().c_str());
//            }
//        }
//        ImGui::End();
    }

    void AssetWidget::BindEvent(EventID id)
    {
        binder.Bind(this, id);
    }

} // namespace sky::editor