//
// Created by blues on 2026/1/11.
//

#pragma once

#include <QDialog>
#include <QListWidget>
#include <framework/asset/AssetCommon.h>

namespace sky::editor {

    class AssetSelectWidget : public QDialog {
    public:
        explicit AssetSelectWidget(const std::string_view &type);

        const AssetSourcePath& GetPath() const { return path; }
    private:
        void OnFilterTextChanged(const QString &text);
        void OnItemDoubleClicked(QListWidgetItem *text);
        void UpdateList(const QString& filter);

        QListWidget* listWidget = nullptr;
        std::string type;
        AssetSourcePath path;
    };

} // namespace sky::editor
