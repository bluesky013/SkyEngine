//
// Created by Zach Lee on 2023/1/14.
//

#include <editor/dockwidget/AssetWidget.h>
#include <QHBoxLayout>

namespace sky::editor {

    AssetWidget::AssetWidget(QWidget *parent) : QDockWidget(parent)
    {
        fsModel = new QFileSystemModel(this);
        fsModel->setRootPath(QDir::currentPath());

        treeView = new QTreeView(this);
        treeView->setModel(fsModel);

        setWindowTitle(tr("Assets"));
        auto widget = new QWidget(this);
        setWidget(widget);
        auto rootLayout = new QHBoxLayout(widget);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setAlignment(Qt::AlignLeft);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);
        rootLayout->addWidget(treeView);
    }

    void AssetWidget::OnProjectChange(const QString &projectPath)
    {
        treeView->setRootIndex(fsModel->setRootPath(projectPath));
    }
}