//
// Created by Zach Lee on 2023/1/14.
//

#include <editor/dockwidget/AssetWidget.h>
#include <QHBoxLayout>
#include <QMenu>
#include <QFileDialog>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>
#include "../window/ActionManager.h"

namespace sky::editor {

    AssetWidget::AssetWidget(QWidget *parent) : QDockWidget(parent)
    {
        const auto &projectFs = AssetDataBase::Get()->GetWorkSpaceFs();
        auto projectPath = QString(projectFs->GetPath().GetFilePath().c_str());

        fsModel = new QFileSystemModel(this);
        fsModel->setRootPath(projectPath);

        treeView = new QTreeView(this);
        treeView->setModel(fsModel);

        treeView->setRootIndex(fsModel->setRootPath(projectPath));

        setWindowTitle(tr("Assets"));
        auto *widget = new QWidget(this);
        setWidget(widget);
        auto *rootLayout = new QHBoxLayout(widget);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setAlignment(Qt::AlignLeft);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);
        rootLayout->addWidget(treeView);

        auto *importAct = new ActionWithFlag(DocumentFlagBit::NONE, tr("Import"), this);
        connect(importAct, &ActionWithFlag::triggered, this, [this]() {
            QFileDialog dialog(this);
            dialog.setFileMode(QFileDialog::AnyFile);
            dialog.setViewMode(QFileDialog::Detail);
            if (dialog.exec() != 0) {
                auto fileNames = dialog.selectedFiles();
                if (!fileNames.empty()) {
                    for (auto &fileName : fileNames) {
//                        AssetManager::Get()->ImportSource(fileName.toStdString(), SourceAssetImportOption{});
                    }
                }
            }
        });

        ActionManager::Get()->AddAction(importAct);

        setContextMenuPolicy(Qt::CustomContextMenu);
        connect(this, &QWidget::customContextMenuRequested, this, [this, importAct](const QPoint &pos) {
            QMenu menu(tr("Assets"), this);
            menu.addAction(importAct);

            menu.exec(mapToGlobal(pos));
        });
    }
}
