
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
    class ViewportWindow;
    class WorldWidget;
    class InspectorContainer;
    class InspectorWidget;
    class AssetWidget;
    class ToolBar;

    class MainWindow : public QMainWindow {
        Q_OBJECT
    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow() override;

        Document* GetDoc() const { return document.get(); }
        ViewportWindow* GetViewportWindow() const;
    private:
        void InitWidgets();
        void InitMenu();
        void InitDocument();

        void OnOpenWorld();
        void OnNewWorld();
        void OnCloseWorld();
        void OnSaveWorld();
        void SaveCheck();

        void OnImport();

        void UpdateActions();

        bool event(QEvent *event) override;

        QMenuBar      *menuBar       = nullptr;
        ToolBar       *toolBar       = nullptr;
        ActionManager *actionManager = nullptr;

        WorldWidget        *worldWidget  = nullptr;
        InspectorContainer *container    = nullptr;
        InspectorWidget    *inspector    = nullptr;
        AssetWidget        *assetBrowser = nullptr;
        ViewportWidget     *mainViewport = nullptr;
        QDockWidget        *toolWidget   = nullptr;
        QWidget            *emptyCentral = nullptr;

        QString projectPath;

        std::vector<ViewportWidget *> viewports;
        std::list<QDockWidget *>      docks;
        std::unique_ptr<Document>     document;
    };

} // namespace sky::editor
