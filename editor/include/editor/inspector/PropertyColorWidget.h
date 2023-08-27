//
// Created by Zach Lee on 2023/8/23.
//

#pragma once

#include <editor/inspector/PropertyWidget.h>
#include <QPushButton>

namespace sky::editor {

    class PropertyColorWidget : public PropertyWidget {
    public:
        explicit PropertyColorWidget(void *inst, const TypeNode *node, QWidget* parent);
        void Refresh() override;

    private:
        QPushButton *button = nullptr;
    };

} // namespace sky::editor