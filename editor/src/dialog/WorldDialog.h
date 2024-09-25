//
// Created by Zach Lee on 2023/1/15.
//

#pragma once

#include <QDialog>
#include <QString>

namespace sky::editor {

    class WorldDialog : public QDialog {
        Q_OBJECT
    public:
        WorldDialog();
        ~WorldDialog() = default;

        const QString &LevelName() const;

    private:
        QString levelName;
    };

} // namespace sky::editor