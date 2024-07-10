//
// Created by Zach Lee on 2023/8/21.
//

#include <editor/inspector/ComponentInspector.h>
#include <framework/world/Component.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor {

    void ComponentInspector::SetComponent(ComponentBase *comp)
    {
        component = comp;

        const auto *node = GetTypeNode(comp->GetTypeId());
        if (node == nullptr) {
            return;
        }
        SetObject(component, node);
    }

} // namespace sky::editor