
//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QMainWindow>
#include <editor/document/Document.h>

class QDockWidget;
class QMenuBar;

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

    private:
        void InitWidgets();
        void InitMenu();

        void OnTick();

        void OnOpenProject(const QString &path);
        void OnNewProject(const QString &path, const QString &name);
        void OnCloseProject();

        void UpdateActions();

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