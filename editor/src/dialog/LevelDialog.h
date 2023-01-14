//
// Created by Zach Lee on 2023/1/15.
//

#pragma once

#include <QDialog>
#include <QString>

namespace sky::editor {

    class LevelDialog : public QDialog {
        Q_OBJECT
    public:
        LevelDialog();
        ~LevelDialog() = default;

        const QString &LevelPath() const;
        const QString &LevelName() const;

    private:
        QString levelPath;
        QString levelName;
    };

} // namespace sky::editor