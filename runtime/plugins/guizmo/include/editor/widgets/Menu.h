//
// Created by Zach on 2024/3/17.
//

#pragma once

#include <list>
#include <memory>
#include <string>
#include <imgui/ImWidget.h>
#include <editor/event/Event.h>

namespace sky::editor {

    class MenuItem : public ImWidget {
    public:
        explicit MenuItem(const std::string &name, EventID id_, bool isBtn_) : ImWidget(name), isBtn(isBtn_), id(id_) {}
        ~MenuItem() override = default;

        void Execute(ImContext &context) override;

    private:
        bool isBtn = true;
        bool selected = false;
        bool enable = true;

        EventID id;
    };

    class Menu : public ImWidget {
    public:
        explicit Menu(const std::string &name) : ImWidget(name) {}
        ~Menu() override = default;

        MenuItem *AddItem(const std::string &name, EventID id, bool isBtn = true);

        void Execute(ImContext &context) override;

    private:
        std::list<std::unique_ptr<MenuItem>> items;
    };

    class MenuBar : public ImWidget {
    public:
        MenuBar() : ImWidget("MenuBar") {}
        ~MenuBar() override = default;

        Menu *AddMenu(const std::string &name);
        void RemoveMenu(Menu *);

        void Execute(ImContext &context) override;

    protected:
        std::list<std::unique_ptr<Menu>> menus;
    };

    class MainMenuBar : public MenuBar {
    public:
        MainMenuBar() = default;
        ~MainMenuBar() override = default;

        void Execute(ImContext &context) override;
    };

} // namespace sky::editor