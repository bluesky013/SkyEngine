//
// Created on 2026/09/21.
//

#pragma once

#include <framework/serialization/SerializationContext.h>
#include <vector>

namespace sky::editor::test {

    struct TestObject {
        float              value = 0.f;
        std::vector<float> items;
    };

    // Mirrors a subsystem config: non-zero member initializers that differ from a
    // zeroed struct, so reset-to-default can be verified field by field.
    struct TestConfig {
        float gravity = -9.81f;
        int   steps   = 4;
        bool  enabled = true;
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
        context->Register<TestObject>("EditorCoreTestObject").Member<&TestObject::value>("value").Member<&TestObject::items>("items");
        context->Register<TestConfig>("EditorCoreTestConfig")
            .Member<&TestConfig::gravity>("gravity")
            .Member<&TestConfig::steps>("steps")
            .Member<&TestConfig::enabled>("enabled");
    }

} // namespace sky::editor::test
