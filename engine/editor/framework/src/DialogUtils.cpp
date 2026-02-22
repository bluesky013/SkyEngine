//
// Created by Zach Lee on 2026/1/11.
//


#include <editor/framework/DialogUtils.h>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace sky::editor {

    NewFileDialog::NewFileDialog(const QString& name, QDialog* parent)
        : QDialog(parent)
    {
        setWindowTitle(name);
        setMinimumWidth(300);
        setModal(true);


        auto label = new QLabel("Name:", this);
        lineEdit = new QLineEdit(this);
        lineEdit->setPlaceholderText("Input File Name...");
        lineEdit->setFocus();  // 自动获得焦点

        auto okButton = new QPushButton("Ok", this);
        auto cancelButton = new QPushButton("Cancel", this);
        okButton->setDefault(true);


        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(label);
        mainLayout->addWidget(lineEdit);

        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);
        mainLayout->addLayout(buttonLayout);

        connect(okButton, &QPushButton::clicked, this, &NewFileDialog::OnOkClicked);
        connect(cancelButton, &QPushButton::clicked, this, &NewFileDialog::reject);
        connect(lineEdit, &QLineEdit::returnPressed, this, &NewFileDialog::OnOkClicked);
    }

    void NewFileDialog::OnOkClicked()
    {
        fileName = lineEdit->text().trimmed();
        if (fileName.isEmpty()) {
            lineEdit->setFocus();
            return;
        }

        accept();
    }
} // namespace sky::editor