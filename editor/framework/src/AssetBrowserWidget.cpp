//
// Created by blues on 2024/7/12.
//

#include <editor/framework/AssetBrowserWidget.h>
#include <framework/asset/AssetBuilderManager.h>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMimeData>
#include <QApplication>
#include <QDrag>
#include <QMenu>
#include <QFileSystemModel>
#include <QComboBox>

namespace sky::editor {

    bool AssetFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
    {
        return true;
    }

    AssetBrowserWidget::AssetBrowserWidget(QWidget *parent) : QWidget(parent)
    {
        setLayout(new QVBoxLayout(this));

        auto *filterWidget = new QWidget(this);
        filterWidget->setLayout(new QHBoxLayout(filterWidget));

        filter = new QLineEdit(filterWidget);
        filter->setClearButtonEnabled(true);
        filter->setPlaceholderText("Search...");

        auto *button = new QPushButton(this);
        button->setFixedWidth(32);
        button->setText("<<");
        connect(button, &QPushButton::clicked, this, &AssetBrowserWidget::ResetView);

        comboBox = new QComboBox(this);
        comboBox->setSizeAdjustPolicy(QComboBox::SizeAdjustPolicy::AdjustToContents);
        connect(comboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int) {
            ResetView();
        });
        comboBox->addItem("Project", static_cast<uint32_t>(SourceAssetBundle::WORKSPACE));
        comboBox->addItem("Engine", static_cast<uint32_t>(SourceAssetBundle::ENGINE));

        filterWidget->layout()->addWidget(comboBox);
        filterWidget->layout()->addWidget(button);
        filterWidget->layout()->addWidget(filter);

        assetItemView = new AssetListView(this);
        assetItemView->setWordWrap(true);
        assetItemView->setViewMode(QListView::IconMode);
        assetItemView->setIconSize(QSize(48, 48));
        assetItemView->setGridSize(QSize(128, 72));
        assetItemView->setUniformItemSizes(true);
        assetItemView->setMovement(QListView::Static);
        assetItemView->setResizeMode(QListView::Adjust);
        assetItemView->setLayoutMode(QListView::Batched);
        assetItemView->setBatchSize(10);
        assetItemView->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
        assetItemView->setContextMenuPolicy(Qt::CustomContextMenu);

        assetItemView->setDragEnabled(true);
        assetItemView->setDragDropMode(QAbstractItemView::DragDropMode::DragDrop);

        auto *model = AssetDataBaseProxy::Get()->GetProjectModel();
        assetItemView->setModel(model);
        assetItemView->setRootIndex(model->setRootPath(model->rootPath()));
        assetItemView->SetAssetBundle(SourceAssetBundle::WORKSPACE);

        connect(assetItemView, &QAbstractItemView::activated, this, [this](const QModelIndex &index) {
            auto *model = static_cast<QFileSystemModel*>(assetItemView->model());
            auto path = model->filePath(index);
            if (QFileInfo(path).isDir()) {
                assetItemView->setRootIndex(model->index(path));
            }
        });

        layout()->addWidget(filterWidget);
        layout()->addWidget(assetItemView);


        connect(assetItemView, &QWidget::customContextMenuRequested, this, &AssetBrowserWidget::OnContentMenuClicked);
    }

    void AssetBrowserWidget::OnContentMenuClicked(const QPoint &pos)
    {
        QMenu menu(tr("Asset Actions"), this);

        auto *buildAct = new QAction(tr("Build"), &menu);
        connect(buildAct, &QAction::triggered, this, [this]() {
            auto indices = assetItemView->selectionModel()->selectedIndexes();
            auto *fsModel = static_cast<QFileSystemModel*>(assetItemView->model());
            QDir root = fsModel->rootPath();
            for (auto &index : indices) {
                auto path = root.relativeFilePath(fsModel->filePath(index));

                AssetSourcePath srcPath = {};
                srcPath.bundle = assetItemView->GetAssetBundle();
                srcPath.path = path.toStdString();
                AssetDataBase::Get()->RegisterAsset(srcPath);
            }
        });

        menu.addAction(buildAct);
        menu.exec(mapToGlobal(pos));
    }

    void AssetBrowserWidget::ResetView()
    {
        if (assetItemView == nullptr) {
            return;
        }

        auto str = comboBox->currentText();
        if (str == "Engine") {
            auto *model = AssetDataBaseProxy::Get()->GetEngineModel();
            assetItemView->setModel(model);
            assetItemView->setRootIndex(model->setRootPath(model->rootPath()));
            assetItemView->SetAssetBundle(SourceAssetBundle::ENGINE);
        } else if (str == "Project") {
            auto *model = AssetDataBaseProxy::Get()->GetProjectModel();
            assetItemView->setModel(model);
            assetItemView->setRootIndex(model->setRootPath(model->rootPath()));
            assetItemView->SetAssetBundle(SourceAssetBundle::WORKSPACE);
        }
    }


    void AssetListView::mousePressEvent(QMouseEvent *event)
    {
        if ((event->buttons() & Qt::LeftButton) != 0) {
            startPos = event->pos();
        }
        QListView::mousePressEvent(event);
    }

    void AssetListView::mouseMoveEvent(QMouseEvent *event)
    {
        if ((event->buttons() & Qt::LeftButton) != 0 &&
            (event->pos() - startPos).manhattanLength() > QApplication::startDragDistance()) {
            QModelIndex i = indexAt(event->pos());

            auto *fsModel = static_cast<QFileSystemModel*>(model());
            QDir root = fsModel->rootPath();
            auto path = root.relativeFilePath(fsModel->filePath(i));
            auto *drag = new QDrag(this);
            auto *mimeData = new QMimeData();

            QString sch = assetBundle == SourceAssetBundle::ENGINE ? "engine:" : "project:";

            mimeData->setUrls({sch + path});;
            drag->setMimeData(mimeData);

            drag->exec(Qt::CopyAction | Qt::MoveAction);
        }
        QListView::mouseMoveEvent(event);
    }

} // namespace sky::editor