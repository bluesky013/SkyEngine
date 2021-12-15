//
// Created by Zach Lee on 2021/12/15.
//

#pragma once

#include <QWidget>

class QLabel;

namespace sky::editor {

    class InspectorBase : public QWidget {
    public:
        InspectorBase(QWidget* parent);
        ~InspectorBase() = default;

    private:
        QLabel* label;
    };

}