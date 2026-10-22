//
// ActionBarBuilder (see ActionBarBuilder.h).
//

#include <editor/widgets/ActionBarBuilder.h>

#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/IconRegistry.h>
#include <editor/core/input/KeyModifiers.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace sky::editor {

    namespace {

        // Parses one accelerator token like "Ctrl+Shift+Z" -> keyCode + modifier flags.
        // Returns false when the token is not understood.
        bool ParseAcceleratorToken(const std::string &token, uint32_t &keyCode, bool &ctrl, bool &shift)
        {
            keyCode = 0;
            ctrl    = false;
            shift   = false;

            std::string part;
            std::string key;
            auto        flush = [&]() {
                if (part.empty()) {
                    return;
                }
                std::string lower;
                for (char c : part) {
                    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                }
                if (lower == "ctrl" || lower == "control") {
                    ctrl = true;
                } else if (lower == "shift") {
                    shift = true;
                } else {
                    key = part;
                }
                part.clear();
            };
            for (char c : token) {
                if (c == '+') {
                    flush();
                } else if (c != ' ') {
                    part.push_back(c);
                }
            }
            flush();

            if (key.empty()) {
                return false;
            }
            if (key.size() == 1) {
                keyCode = static_cast<uint32_t>(static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(key[0]))));
                return true;
            }
            std::string lower;
            for (char c : key) {
                lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            if (lower == "esc" || lower == "escape") {
                keyCode = 0x1B;
            } else if (lower == "enter" || lower == "return") {
                keyCode = 0x0D;
            } else if (lower == "space") {
                keyCode = 0x20;
            } else if (lower == "tab") {
                keyCode = 0x09;
            } else if (lower == "delete" || lower == "del") {
                keyCode = 0x2E;
            } else if (lower == "backspace") {
                keyCode = 0x08;
            } else if (lower.size() >= 2 && lower[0] == 'f') {
                const int n = std::atoi(lower.c_str() + 1);
                if (n >= 1 && n <= 12) {
                    keyCode = static_cast<uint32_t>(0x70 + (n - 1)); // VK_F1..VK_F12
                } else {
                    return false;
                }
            } else {
                return false;
            }
            return true;
        }

        bool MatchesToken(const std::string &token, const sky::ui::UIKeyEvent &event)
        {
            uint32_t keyCode = 0;
            bool     ctrl    = false;
            bool     shift   = false;
            if (!ParseAcceleratorToken(token, keyCode, ctrl, shift)) {
                return false;
            }
            if (event.keyCode != keyCode) {
                return false;
            }
            return ctrl == ((event.modifiers & kModCtrl) != 0) && shift == ((event.modifiers & kModShift) != 0);
        }

        // The first alternative, shown as the menu shortcut hint.
        std::string DisplayShortcut(const std::string &accelerator)
        {
            const auto  comma = accelerator.find(',');
            std::string token = comma == std::string::npos ? accelerator : accelerator.substr(0, comma);
            while (!token.empty() && token.front() == ' ') {
                token.erase(token.begin());
            }
            while (!token.empty() && token.back() == ' ') {
                token.pop_back();
            }
            return token;
        }

        bool MatchesAccelerator(const std::string &accelerator, const sky::ui::UIKeyEvent &event)
        {
            std::string token;
            for (char c : accelerator) {
                if (c == ',') {
                    if (MatchesToken(token, event)) {
                        return true;
                    }
                    token.clear();
                } else {
                    token.push_back(c);
                }
            }
            return MatchesToken(token, event);
        }

    } // namespace

    std::vector<ToolBar::Item> ActionBarBuilder::BuildToolBarItems(const Invoker &invoke)
    {
        std::vector<ToolBar::Item> items;
        EditorGroup                lastGroup = EditorGroup::None;
        auto                       snapshots = EditorActionRegistry::Get()->GetActionSnapshots();
        std::sort(snapshots.begin(), snapshots.end(), [](const auto &a, const auto &b) {
            if (a.action.group != b.action.group)
                return a.action.group < b.action.group;
            if (a.action.order != b.action.order)
                return a.action.order < b.action.order;
            return a.action.id < b.action.id;
        });
        for (const auto &snapshot : snapshots) {
            const EditorAction &action = snapshot.action;
            if (action.group == EditorGroup::None)
                continue;
            ToolBar::Item item;
            item.id    = action.id;
            item.label = action.label;
            if (!action.icon.empty()) {
                if (const uint32_t icon = IconRegistry::Get()->Find(action.icon); icon != 0) {
                    item.icon = icon;
                }
            }
            const std::string id = action.id;
            item.action          = [invoke, id, snapshot]() {
                EditorActionRegistry::Get()->Invoke(snapshot, [invoke, id]() {
                    if (invoke)
                        invoke(id);
                });
            };
            item.resolveState    = [snapshot]() { return EditorActionRegistry::Get()->ResolveState(snapshot); };
            const auto state     = item.resolveState();
            item.enabled         = state.enabled;
            item.visible         = state.visible;
            item.checkable       = state.checkable;
            item.checked         = state.checked;
            item.separatorBefore = lastGroup != EditorGroup::None && action.group != lastGroup;
            lastGroup            = action.group;
            items.push_back(std::move(item));
        }
        return items;
    }

    std::vector<MenuBar::Menu> ActionBarBuilder::BuildMenus(const Invoker &invoke)
    {
        std::vector<MenuBar::Menu> menus;
        MenuBar::Menu              current;
        EditorMenu                 currentMenu = EditorMenu::None;
        auto                       flush       = [&menus, &current]() {
            if (!current.label.empty()) {
                menus.push_back(std::move(current));
                current = MenuBar::Menu{};
            }
        };
        auto snapshots = EditorActionRegistry::Get()->GetActionSnapshots();
        std::sort(snapshots.begin(), snapshots.end(), [](const auto &a, const auto &b) {
            if (a.action.menu != b.action.menu)
                return a.action.menu < b.action.menu;
            if (a.action.order != b.action.order)
                return a.action.order < b.action.order;
            return a.action.id < b.action.id;
        });
        for (const auto &snapshot : snapshots) {
            const EditorAction &action = snapshot.action;
            if (action.menu == EditorMenu::None)
                continue;
            if (action.menu != currentMenu) {
                flush();
                currentMenu   = action.menu;
                const char *l = EditorMenuLabel(action.menu);
                current.label = l != nullptr ? l : std::string{};
            }
            const std::string id = action.id;
            MenuBar::Item     item;
            item.label  = action.label;
            item.action = [invoke, id, snapshot]() {
                EditorActionRegistry::Get()->Invoke(snapshot, [invoke, id]() {
                    if (invoke)
                        invoke(id);
                });
            };
            item.resolveState = [snapshot]() { return EditorActionRegistry::Get()->ResolveState(snapshot); };
            const auto state  = item.resolveState();
            item.enabled      = state.enabled;
            item.visible      = state.visible;
            item.checkable    = state.checkable;
            item.checked      = state.checked;
            item.shortcut     = DisplayShortcut(action.accelerator);
            item.icon         = action.icon;
            if (action.submenu.empty()) {
                current.items.push_back(std::move(item));
            } else {
                const auto it = std::find_if(current.items.begin(), current.items.end(),
                                             [&](const MenuBar::Item &existing) { return existing.label == action.submenu; });
                if (it == current.items.end()) {
                    MenuBar::Item sub;
                    sub.label = action.submenu;
                    sub.children.push_back(std::move(item));
                    current.items.push_back(std::move(sub));
                } else {
                    it->children.push_back(std::move(item));
                }
            }
        }
        flush();
        return menus;
    }

    std::string ActionBarBuilder::MatchAccelerator(const sky::ui::UIKeyEvent &event)
    {
        if (event.action != sky::ui::UIKeyAction::DOWN) {
            return {};
        }
        for (const auto &snapshot : EditorActionRegistry::Get()->GetActionSnapshots()) {
            const EditorAction &action = snapshot.action;
            if (!action.accelerator.empty() && MatchesAccelerator(action.accelerator, event)) {
                if (action.acceleratorEligible && !action.acceleratorEligible())
                    continue;
                const auto state = EditorActionRegistry::Get()->ResolveState(snapshot);
                if (state.visible)
                    return action.id;
            }
        }
        return {};
    }

} // namespace sky::editor
