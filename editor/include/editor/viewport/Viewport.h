//
// Created by Zach Lee on 2021/12/13.
//

#pragma once

#include <QWindow>
#include <engine/world/Viewport.h>

namespace sky::editor {

    class Viewport : public QWindow {
        Q_OBJECT
    public:
        Viewport();
        ~Viewport();

        void Init();

        sky::Viewport* GetNativeViewport() const;

        bool event(QEvent *event) override;

    private:
        sky::Viewport* viewport;
    };

}