//
// Created by Zach Lee on 2026/1/11.
//

#pragma once

#include <QDialog>

class QLineEdit;

namespace sky::editor {

    class NewFileDialog : public QDialog {
        Q_OBJECT
    public:
        explicit NewFileDialog(const QString& name, QDialog* parent = nullptr);

        const QString& GetFileName() const { return fileName; }
    private:
        void OnOkClicked();

        QString fileName;

        QLineEdit* lineEdit = nullptr;
    };

} // namespace sky::editor