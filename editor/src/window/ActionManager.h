//
// Created by Zach Lee on 2022/8/11.
//

#pragma once

#include <QMap>
#include <QString>
#include <QAction>
#include <bitset>

namespace sky::editor {

    using ActionFlag = std::bitset<64>;

    class ActionWithFlag : public QAction {
    public:
        explicit ActionWithFlag(const ActionFlag &flag, QObject *parent = nullptr);
        explicit ActionWithFlag(const ActionFlag &flag, const QString &text, QObject *parent = nullptr);
        explicit ActionWithFlag(const ActionFlag &flag, const QIcon &icon, const QString &text, QObject *parent = nullptr);

        void Update(const ActionFlag& flag);

    private:
        ActionFlag flag;
    };

    class ActionManager {
    public:
        ActionManager() = default;
        ~ActionManager() = default;

        void AddAction(ActionWithFlag* action);

        void Update(const ActionFlag& flag);

    private:
        QMap<QString, ActionWithFlag*> actions;
    };

}