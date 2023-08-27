//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <QWidget>
#include <editor/viewport/Viewport.h>

namespace sky::editor {

    class ViewportWidget : public QWidget {
        Q_OBJECT
    public:
        explicit ViewportWidget(QWidget* parent = nullptr);
        ~ViewportWidget() override = default;

        void Init();

    private:
        Viewport* window;
    };
}
