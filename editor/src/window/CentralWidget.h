//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QMainWindow>
#include <editor/viewport/ViewportWidget.h>

namespace sky::editor {

    class CentralWidget : public QWidget {
    public:
        CentralWidget(QWidget* parent = nullptr);
        ~CentralWidget();

        void Init();

        void Shutdown();

        ViewportWidget* GetViewport() const;

    private:
        ViewportWidget* viewport = nullptr;
    };

}