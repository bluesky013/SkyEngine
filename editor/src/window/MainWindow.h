
//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QMainWindow>
#include <editor/document/Document.h>

class QDockWidget;
class QMenuBar;
class QTimer;

namespace sky {
    class SkyEngine;
}

namespace sky::editor {

    class ActionManager;
    class ViewportWidget;
    class WorldWidget;
    class InspectorWidget;
    class AssetWidget;

    class MainWindow : public QMainWindow {
        Q_OBJECT
    public:
        MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

        void Init();

    private:
        void InitEngine();
        void InitMenu();

        void ShutdownEngine();

        void OnTick();

        void OnOpenProject(const QString &path);
        void OnNewProject(const QString &path, const QString &name);
        void OnCloseProject();

        void UpdateActions();

        SkyEngine     *engine        = nullptr;
        QTimer        *timer         = nullptr;
        QMenuBar      *menuBar       = nullptr;
        ActionManager *actionManager = nullptr;

        WorldWidget     *worldWidget  = nullptr;
        InspectorWidget *inspector    = nullptr;
        AssetWidget     *assetBrowser = nullptr;

        std::vector<ViewportWidget *> viewports;
        std::list<QDockWidget *>      docks;
        std::unique_ptr<Document>     document;
    };

} // namespace sky::editor