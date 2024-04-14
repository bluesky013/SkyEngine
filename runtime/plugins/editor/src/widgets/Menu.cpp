//
// Created by blues on 2024/3/17.
//

#include <editor/widgets/Menu.h>
#include <imgui.h>

namespace sky::editor {

    void MenuItem::Execute(ImContext &context)
    {
        if (isBtn) {
            if (ImGui::MenuItem(name.c_str(), nullptr, false, enable)) {
                ButtonEvent::BroadCast(id, &IButtonEvent::OnClicked);
            }
        } else {
            if (ImGui::MenuItem(name.c_str(), nullptr, &selected, enable)) {
                ToogleEvent::BroadCast(id, &IToggleEvent::OnToggle, selected);
            }
        }
    }

    void Menu::Execute(ImContext &context)
    {
        if (ImGui::BeginMenu(name.c_str())) {
            for (auto &item: items) {
                item->Execute(context);
            }

            ImGui::EndMenu();
        }
    }

    MenuItem *Menu::AddItem(const std::string &name, EventID id, bool isBtn)
    {
        items.emplace_back(std::make_unique<MenuItem>(name, id, isBtn));
        return items.back().get();
    }

    void MenuBar::Execute(ImContext &context)
    {
        if (ImGui::BeginMenuBar())
        {
            for (auto &menu : menus) {
                menu->Execute(context);
            }
            ImGui::EndMenuBar();
        }
    }

    void MainMenuBar::Execute(ImContext &context)
    {
        if (ImGui::BeginMainMenuBar())
        {
            for (auto &menu : menus) {
                menu->Execute(context);
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