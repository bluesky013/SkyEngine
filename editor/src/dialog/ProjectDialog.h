//
// Created by Zach Lee on 2023/1/14.
//

#pragma once

#include <QDialog>
#include <QString>

namespace sky::editor {

    class ProjectDialog : public QDialog {
        Q_OBJECT
    public:
        ProjectDialog();
        ~ProjectDialog() = default;

        const QString &ProjectPath() const;
        const QString &ProjectName() const;

    private:
        QString projectPath;
        QString projectName;
    };

} // namespace sky::editor