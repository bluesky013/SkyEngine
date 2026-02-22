//
// Created by blues on 2026/1/11.
//

#include <editor/framework/AssetSelectWidget.h>
#include <framework/asset/AssetDataBase.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>

namespace sky::editor {

    AssetSelectWidget::AssetSelectWidget(const std::string_view &inType)
        : type(inType)
    {
        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(5);

        auto *filterLabel = new QLabel("🔍");
        filterLabel->setFixedWidth(20);

        auto *filterLayout = new QHBoxLayout();
        auto *filterEdit = new QLineEdit();
        filterEdit->setPlaceholderText(tr("Type to filter..."));
        filterEdit->setClearButtonEnabled(true);

        filterLayout->addWidget(filterLabel);
        filterLayout->addWidget(filterEdit);

        listWidget = new QListWidget();
        listWidget->setAlternatingRowColors(true);

        mainLayout->addLayout(filterLayout);
        mainLayout->addWidget(listWidget);

        connect(filterEdit, &QLineEdit::textChanged, this, &AssetSelectWidget::OnFilterTextChanged);
        connect(listWidget, &QListWidget::itemDoubleClicked, this, &AssetSelectWidget::OnItemDoubleClicked);
//        connect(listWidget, &QListWidget::itemSelectionChanged, this, &FilterListWidget::onSelectionChanged);
    }

    void AssetSelectWidget::OnFilterTextChanged(const QString &text)
    {
        UpdateList(text);
    }

    void AssetSelectWidget::OnItemDoubleClicked(QListWidgetItem *item)
    {
        QString asset = item->data(Qt::UserRole + 1).toString();
        path.bundle = (SourceAssetBundle)item->data(Qt::UserRole).toUInt();
        path.path = FilePath(asset.toStdString());

        accept();
    }

    void AssetSelectWidget::UpdateList(const QString &text)
    {
        listWidget->clear();

        std::string filter = text.toStdString();

        std::vector<AssetSourcePtr> typedAssets = AssetDataBase::Get()->Gather(type);

        for (auto& source : typedAssets) {
            if (filter.empty() || source->path.path.FileName().find(filter) != std::string::npos) {
                auto str = QString(source->path.path.GetStr().c_str());

                auto *listItem = new QListWidgetItem(str);
                listItem->setData(Qt::UserRole, (uint32_t)source->path.bundle);
                listItem->setData(Qt::UserRole + 1, str);
                listWidget->addItem(listItem);
            }
        }
    }

} // namespace sky::editor
