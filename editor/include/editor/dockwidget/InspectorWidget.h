//
// Created by Zach Lee on 2021/12/15.
//

#pragma once

#include <QDockWidget>

namespace sky {
    class World;
    class GameObject;
}

namespace sky::editor {

    class InspectorWidget : public QDockWidget {
    public:
        InspectorWidget(QWidget* parent);
        ~InspectorWidget() = default;
    };

}