//
// Created by Zach Lee on 2026/1/11.
//

#pragma once

#include <QDialog>
#include <functional>

class QLineEdit;
class QLabel;
class QProgressBar;

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

    /**
     * @brief Modal (blocking) progress dialog.
     *
     * Displays a progress bar and status text while a long-running operation
     * executes. The dialog pumps the Qt event loop so the UI stays responsive.
     *
     * Usage:
     * @code
     *   ProgressDialog dlg("Baking PVS...");
     *   dlg.SetRange(0, totalSteps);
     *   dlg.Show();
     *
     *   for (int i = 0; i < totalSteps; ++i) {
     *       DoWork(i);
     *       dlg.SetValue(i + 1);
     *       dlg.SetStatusText(QString("Processing step %1/%2").arg(i+1).arg(totalSteps));
     *       if (dlg.WasCancelled()) break;
     *   }
     *
     *   dlg.Finish();
     * @endcode
     */
    class ProgressDialog : public QDialog {
        Q_OBJECT
    public:
        explicit ProgressDialog(const QString &title, QWidget *parent = nullptr);
        ~ProgressDialog() override = default;

        /** Set the min/max range of the progress bar. */
        void SetRange(int min, int max);

        /** Set the current progress value and pump the event loop. */
        void SetValue(int value);

        /** Update the status text below the progress bar. */
        void SetStatusText(const QString &text);

        /** Show the dialog (modal, but non-blocking ¡ª caller drives the loop). */
        void Show();

        /** Close the dialog when the work is done. */
        void Finish();

        /** Returns true if the user pressed Cancel. */
        bool WasCancelled() const { return cancelled; }

    private:
        void OnCancelClicked();

        QProgressBar *progressBar = nullptr;
        QLabel       *statusLabel = nullptr;
        bool          cancelled   = false;
    };

} // namespace sky::editor