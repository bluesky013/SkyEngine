//
// Created by blues on 2024/3/17.
//

#include <editor/widgets/Menu.h>
#include <imgui.h>

namespace sky::editor {

    void MenuBar::Render(float time)
    {
        if (ImGui::BeginMainMenuBar())
        {
            for (auto &menu : menus) {
                if (ImGui::BeginMenu(name.c_str()))
                {
//                    for (auto &item : items) {
//                        if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
//                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMainMenuBar();
        }
    }

    Menu *MenuBar::AddMenu(const std::string &name)
    {
        menus.emplace_back(std::make_unique<Menu>(name));
        return menus.back().get();
    }

    void MenuBar::RemoveMenu(Menu *menu)
    {
        menus.remove_if([menu](const auto &v) {
            return menu == v.get();
        });
    }
} // namespace sky::editor