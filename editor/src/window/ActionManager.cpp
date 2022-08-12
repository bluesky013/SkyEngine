//
// Created by Zach Lee on 2022/8/11.
//

#include "ActionManager.h"

namespace sky::editor {

    ActionWithFlag::ActionWithFlag(const ActionFlag &value, QObject *parent)
        : QAction(parent)
        , flag(value)
    {
    }

    ActionWithFlag::ActionWithFlag(const ActionFlag &value, const QString &text, QObject *parent)
        : QAction(text, parent)
        , flag(value)
    {
    }

    ActionWithFlag::ActionWithFlag(const ActionFlag &value, const QIcon &icon, const QString &text, QObject *parent)
        : QAction(icon, text, parent)
        , flag(value)
    {
    }

    void ActionWithFlag::Update(const ActionFlag& value)
    {
        setEnabled((flag & value) == flag);
    }

    void ActionManager::AddAction(ActionWithFlag* action)
    {
        actions.insert(action->text(), action);
    }

    void ActionManager::Update(const ActionFlag& flag)
    {
        for(auto& action : actions) {
            action->Update(flag);
        }
    }

}