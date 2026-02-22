//
// Created by Zach Lee on 2023/8/21.
//

#pragma once

#include <editor/inspector/InspectorBase.h>
#include <framework/world/Component.h>

namespace sky::editor {

    class ComponentInspector : public InspectorBase {
    public:
        explicit ComponentInspector(QWidget* parent) : InspectorBase(parent) {}
        ~ComponentInspector() override = default;

        void SetComponent(ComponentBase *comp);

    private:
        ComponentBase *component = nullptr;
    };

} // namespace sky::editor