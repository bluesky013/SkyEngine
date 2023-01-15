//
// Created by Zach Lee on 2022/8/11.
//

#pragma once

#include <QAction>
#include <QMap>
#include <QString>
#include <editor/document/Constants.h>

namespace sky::editor {

    class ActionWithFlag : public QAction {
    public:
        explicit ActionWithFlag(const DocFlagArray &flag, QObject *parent = nullptr);
        explicit ActionWithFlag(const DocFlagArray &flag, const QString &text, QObject *parent = nullptr);
        explicit ActionWithFlag(const DocFlagArray &flag, const QIcon &icon, const QString &text, QObject *parent = nullptr);

        void Update(const DocFlagArray &value);

    private:
        DocFlagArray flags;
    };

    class ActionManager {
    public:
        ActionManager()  = default;
        ~ActionManager() = default;

        void AddAction(ActionWithFlag *action);

        void Update(const DocFlagArray &flag);

    private:
        QMap<QString, ActionWithFlag *> actions;
    };

} // namespace sky::editor