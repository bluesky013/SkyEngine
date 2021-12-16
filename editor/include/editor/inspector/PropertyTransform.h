//
// Created by Zach Lee on 2021/12/16.
//

#pragma once
#include <QWidget>
#include <editor/inspector/PropertyWidget.h>

namespace sky::editor {

    class PropertyTransform : public PropertyWidget {
    public:
        PropertyTransform(QWidget* parent);
        ~PropertyTransform() = default;

        void SetInstance(void* instance, const QString&, const TypeMemberNode& node) override;

    private:
        PropertyWidget* pos;
        PropertyWidget* scale;
        PropertyWidget* rotation;
    };

}