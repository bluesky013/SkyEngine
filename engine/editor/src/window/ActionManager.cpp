//
// Created by Zach Lee on 2022/8/11.
//

#include "ActionManager.h"

namespace sky::editor {

    ActionWithFlag::ActionWithFlag(const DocFlagArray &value, QObject *parent) : QAction(parent), flags(value)
    {
    }

    ActionWithFlag::ActionWithFlag(const DocFlagArray &value, const QString &text, QObject *parent) : QAction(text, parent), flags(value)
    {
    }

    ActionWithFlag::ActionWithFlag(const DocFlagArray &value, const QIcon &icon, const QString &text, QObject *parent)
        : QAction(icon, text, parent), flags(value)
    {
    }

    void ActionWithFlag::Update(const DocFlagArray &value)
    {
        setEnabled((flags & value) == flags);
    }

    void ActionManager::AddAction(ActionWithFlag *action)
    {
        actions.insert(action->text(), action);
    }

    void ActionManager::Update(const DocFlagArray &flag)
    {
        for (auto &action : actions) {
            action->Update(flag);
        }
    }

} // namespace sky::editor