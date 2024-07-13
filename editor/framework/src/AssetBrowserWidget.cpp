//
// Created by blues on 2024/7/12.
//

#include <editor/framework/AssetBrowserWidget.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>

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

        categoryBox = new QComboBox(filterWidget);
        const auto &table = AssetDataBaseProxy::Get()->GetCategoryTable();
        for (const auto &[key, data] : table) {
            categoryBox->addItem(key.c_str());
        }

        filterWidget->layout()->addWidget(categoryBox);
        filterWidget->layout()->addWidget(filter);

        assetItemView = new QListView(this);

        layout()->addWidget(filterWidget);
        layout()->addWidget(assetItemView);
    }

} // namespace sky::editor