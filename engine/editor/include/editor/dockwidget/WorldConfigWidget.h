//
// Created by Zach Lee on 2026/2/24.
//

#pragma once

#include <QDockWidget>
#include <QTimer>
#include <framework/world/World.h>

namespace sky::editor {

    class WorldConfigWidget : public QDockWidget {
    public:
        explicit WorldConfigWidget(QWidget* parent = nullptr);
        ~WorldConfigWidget() override = default;

        void SetWorld(const WorldPtr& world);
    };

}