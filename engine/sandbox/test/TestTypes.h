//
// Created on 2026/09/21.
//

#pragma once

#include <framework/serialization/SerializationContext.h>
#include <vector>

namespace sky::editor::test {

    struct TestObject {
        float value = 0.f;
        std::vector<float> items;
    };

    // Registers the reflection metadata used by the EditorCore tests. Safe to
    // call from every test; registration happens once.
    inline void RegisterTestTypes()
    {
        static bool registered = false;
        if (registered) {
            return;
        }
        registered = true;

        auto *context = SerializationContext::Get();
        context->Register<TestObject>("EditorCoreTestObject")
            .Member<&TestObject::value>("value")
            .Member<&TestObject::items>("items");
    }

} // namespace sky::editor::test
