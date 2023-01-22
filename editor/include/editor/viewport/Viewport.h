//
// Created by Zach Lee on 2021/12/13.
//

#pragma once

#include <QWindow>

namespace sky::editor {

    class Viewport : public QWindow {
        Q_OBJECT
    public:
        Viewport();
        ~Viewport();

        void Init();

        bool event(QEvent *event) override;

    private:
    };

}
