
//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QMainWindow>

namespace sky::editor {

    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

        void Init();

    private:
    };

}