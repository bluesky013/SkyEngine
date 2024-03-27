//
// Created by Zach on 2024/3/17.
//

#pragma once

#include "IWidget.h"
#include <list>
#include <memory>
#include <string>
#include <imgui/ImGuiInstance.h>

namespace sky::editor {

    class MenuItem : public IWidget {
    public:
        explicit MenuItem(const std::string &name) : IWidget(name) {}
        ~MenuItem() override = default;
    };

    class Menu : public IWidget {
    public:
        explicit Menu(const std::string &name) : IWidget(name) {}
        ~Menu() override = default;

    private:
        std::list<std::unique_ptr<MenuItem>> items;
    };

    class MenuBar : public IWidget {
    public:
        MenuBar() : IWidget("MenuBar") {}
        ~MenuBar() override = default;

        Menu *AddMenu(const std::string &name);
        void RemoveMenu(Menu *);

        void Render(float time);
    private:
        std::list<std::unique_ptr<Menu>> menus;
    };

} // namespace sky::editor