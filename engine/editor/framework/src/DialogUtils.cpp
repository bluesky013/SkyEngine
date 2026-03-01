//
// Created by Zach Lee on 2026/1/11.
//


#include <editor/framework/DialogUtils.h>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QApplication>

namespace sky::editor {

    // =========================================================================
    // NewFileDialog
    // =========================================================================

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

    // =========================================================================
    // ProgressDialog
    // =========================================================================

    ProgressDialog::ProgressDialog(const QString &title, QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(title);
        setMinimumWidth(400);
        setModal(true);

        // Disable the close button so the user must use Cancel
        setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

        statusLabel = new QLabel(this);
        statusLabel->setText(tr("Please wait..."));

        progressBar = new QProgressBar(this);
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        progressBar->setTextVisible(true);

        auto *cancelButton = new QPushButton(tr("Cancel"), this);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(statusLabel);
        mainLayout->addWidget(progressBar);

        auto *buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        buttonLayout->addWidget(cancelButton);
        mainLayout->addLayout(buttonLayout);

        connect(cancelButton, &QPushButton::clicked, this, &ProgressDialog::OnCancelClicked);
    }

    void ProgressDialog::SetRange(int min, int max)
    {
        progressBar->setRange(min, max);
    }

    void ProgressDialog::SetValue(int value)
    {
        progressBar->setValue(value);
        // Pump the event loop so the UI repaints and stays responsive
        QApplication::processEvents();
    }

    void ProgressDialog::SetStatusText(const QString &text)
    {
        statusLabel->setText(text);
        QApplication::processEvents();
    }

    void ProgressDialog::Show()
    {
        cancelled = false;
        show();
        QApplication::processEvents();
    }

    void ProgressDialog::Finish()
    {
        accept();
    }

    void ProgressDialog::OnCancelClicked()
    {
        cancelled = true;
    }

} // namespace sky::editor