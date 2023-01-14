//
// Created by Zach Lee on 2023/1/14.
//

#include "ProjectDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>

namespace sky::editor {

    ProjectDialog::ProjectDialog()
    {
        setWindowTitle(tr("New Project"));
        setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);

        auto layout = new QVBoxLayout(this);
        auto pathGroup = new QWidget(this);
        auto projectPathText = new QLineEdit(pathGroup);
        auto projectPathBtn = new QPushButton(tr("..."), pathGroup);
        connect(projectPathBtn, &QPushButton::clicked, this, [projectPathText, this](bool checked) {
            QFileDialog dlg;
            dlg.setFileMode(QFileDialog::Directory);
            dlg.setViewMode(QFileDialog::Detail);
            QStringList fileNames;
            if (dlg.exec()) {
                fileNames = dlg.selectedFiles();
                if (!fileNames.empty()) {
                    projectPath = fileNames[0];
                    projectPathText->setText(projectPath);
                }
            }
        });

        auto pathGroupLayout = new QHBoxLayout(pathGroup);
        pathGroupLayout->setContentsMargins(0, 0, 0, 0);
        pathGroupLayout->addWidget(projectPathText);
        pathGroupLayout->addWidget(projectPathBtn);

        auto newGroup = new QWidget(this);
        auto projectNameText = new QLineEdit(newGroup);
        auto projectNameBtn = new QPushButton(tr("New"), newGroup);
        connect(projectNameBtn, &QPushButton::clicked, this, [projectNameText, this](bool checked) {
            projectName = projectNameText->text();
            accept();
        });
        auto nameGroupLayout = new QHBoxLayout(newGroup);
        nameGroupLayout->setContentsMargins(0, 0, 0, 0);
        nameGroupLayout->addWidget(projectNameText);
        nameGroupLayout->addWidget(projectNameBtn);

        layout->addWidget(new QLabel(tr("Project Path:"), this));
        layout->addWidget(pathGroup);
        layout->addWidget(new QLabel(tr("Project Name:"), this));
        layout->addWidget(newGroup);
        setLayout(layout);
    }

    const QString &ProjectDialog::ProjectPath() const
    {
        return projectPath;
    }

    const QString &ProjectDialog::ProjectName() const
    {
        return projectName;
    }
} // namespace sky::editor