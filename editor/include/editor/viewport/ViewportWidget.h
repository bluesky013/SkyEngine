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
        ViewportWidget(QWidget* parent = nullptr);
        ~ViewportWidget();

        void Init();

        sky::Viewport* GetNativeViewport() const;

    private:
        Viewport* window;
    };

}
