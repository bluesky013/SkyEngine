//
// Created by Zach Lee on 2023/1/14.
//

#include <editor/dockwidget/AssetWidget.h>
#include <QHBoxLayout>
#include <QMenu>
#include <QFileDialog>
#include <QSplitter>
#include <editor/framework/AssetBrowserWidget.h>
#include <editor/framework/AssetEditorProxy.h>
#include "../window/ActionManager.h"

namespace sky::editor {

    AssetDirBrowser::AssetDirBrowser(QWidget *parent) : QFrame(parent)
    {
        const auto &projectFs = AssetDataBase::Get()->GetWorkSpaceFs();
        auto projectPath = QString(projectFs->GetPath().GetStr().c_str());

        const auto &engineFs = AssetDataBase::Get()->GetEngineFs();
        auto enginePath = QString(engineFs->GetPath().GetStr().c_str());

        auto *fsModel = new QFileSystemModel(this);
        fsModel->setRootPath(projectPath);

        auto *engineModel = new QFileSystemModel(this);
        engineModel->setRootPath(enginePath);

        modelRoot[static_cast<uint32_t>(SourceAssetBundle::WORKSPACE)] = fsModel->setRootPath(projectPath);
        modelRoot[static_cast<uint32_t>(SourceAssetBundle::ENGINE)] = engineModel->setRootPath(enginePath);

        models.emplace(static_cast<uint32_t>(SourceAssetBundle::WORKSPACE), fsModel);
        models.emplace(static_cast<uint32_t>(SourceAssetBundle::ENGINE), engineModel);

        treeView = new QTreeView(this);
        treeView->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);

        auto *comboBox = new QComboBox(this);
        comboBox->setSizeAdjustPolicy(QComboBox::SizeAdjustPolicy::AdjustToContents);
        connect(comboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
        [this, comboBox](int index) {
            auto id = comboBox->itemData(index).value<uint32_t>();
            treeView->setModel(models[id]);
            treeView->setRootIndex(modelRoot[id]);
            currentModel = id;
        });

        comboBox->addItem("Engine", static_cast<uint32_t>(SourceAssetBundle::ENGINE));
        comboBox->addItem("Project", static_cast<uint32_t>(SourceAssetBundle::WORKSPACE));

        setLayout(new QVBoxLayout(this));
        layout()->addWidget(comboBox);
        layout()->addWidget(treeView);

        setContextMenuPolicy(Qt::CustomContextMenu);
        connect(this, &QWidget::customContextMenuRequested, this, &AssetDirBrowser::OnContentMenuClicked);
    }

    void AssetDirBrowser::OnContentMenuClicked(const QPoint &pos)
    {
        QMenu menu(tr("World Action"), this);

        auto *reloadAct = new QAction(tr("Reload"), &menu);

        auto indices = treeView->selectionModel()->selectedIndexes();
        auto *model = models[currentModel];
        if (model == nullptr || indices.empty()) {
            return;
        }
        auto rootPath = QDir(model->rootPath());
        AssetSourcePath assetSourcePath;
        assetSourcePath.bundle = static_cast<SourceAssetBundle>(currentModel);

        for (auto index : indices) {
            if (index.column() != 0) {
                continue;
            }

            QFileInfo file(static_cast<QFileSystemModel*>(treeView->model())->filePath(index));
            if (file.isFile()) {
                assetSourcePath.path = rootPath.relativeFilePath(file.filePath()).toStdString();
            }
        }

        if (assetSourcePath.path.empty()) {
            return;
        }

        connect(reloadAct, &QAction::triggered, this, [assetSourcePath]() {
            AssetDataBase::Get()->RegisterAsset(assetSourcePath);
        });

        menu.addAction(reloadAct);
        menu.exec(mapToGlobal(pos));
    }

    AssetWidget::AssetWidget(QWidget *parent) : QDockWidget(parent)
    {
        setWindowTitle(tr("Assets"));
        auto *widget = new QWidget(this);
        setWidget(widget);

        auto *splitter = new QSplitter(this);
        splitter->addWidget(new AssetDirBrowser(this));
        splitter->addWidget(new AssetBrowserWidget(this));

        auto *rootLayout = new QHBoxLayout(widget);
        rootLayout->addWidget(splitter);

//        auto *importAct = new ActionWithFlag(DocumentFlagBit::ProjectOpen, tr("Import"), this);
//        connect(importAct, &ActionWithFlag::triggered, this, [this]() {
//            QFileDialog dialog(this);
//            dialog.setFileMode(QFileDialog::AnyFile);
//            dialog.setViewMode(QFileDialog::Detail);
//            if (dialog.exec() != 0) {
//                auto fileNames = dialog.selectedFiles();
//                if (!fileNames.empty()) {
//                    for (auto &fileName : fileNames) {
////                        AssetDataBase::Get()->ImportSource(fileName.toStdString(), SourceAssetImportOption{});
//                    }
//                }
//            }
//        });
//
//        ActionManager::Get()->AddAction(importAct);

//        setContextMenuPolicy(Qt::CustomContextMenu);
//        connect(this, &QWidget::customContextMenuRequested, this, [this, importAct](const QPoint &pos) {
//            QMenu menu(tr("Assets"), this);
//            menu.addAction(importAct);
//
//            menu.exec(mapToGlobal(pos));
//        });
    }
}
