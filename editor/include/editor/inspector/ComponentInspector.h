//
// Created by Zach Lee on 2023/8/21.
//

#pragma once

#include <editor/inspector/InspectorBase.h>

namespace sky {
    class Component;
} // namespace sky

namespace sky::editor {

    class ComponentInspector : public InspectorBase {
    public:
        explicit ComponentInspector(QWidget* parent) : InspectorBase(parent) {}
        ~ComponentInspector() override = default;

        void SetComponent(Component *comp);

    private:
        Component *component = nullptr;
    };

} // namespace sky::editor