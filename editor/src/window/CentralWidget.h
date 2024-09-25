//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QMainWindow>
#include <QVBoxLayout>

namespace sky::editor {

    class CentralWidget : public QWidget {
    public:
        explicit CentralWidget(QWidget *parent = nullptr);
        ~CentralWidget() override;

        void Init();
    private:
        void Update();

        QVBoxLayout *layout = nullptr;
    };

} // namespace sky::editor