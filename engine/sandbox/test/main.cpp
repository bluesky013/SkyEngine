//
// Created on 2026/09/21.
//

#include "TestTypes.h"
#include <editor/core/preferences/EditorSettings.h>
#include <editor/core/property/ReflectionDemoTypes.h>
#include <editor/core/viewport/EditorCamera.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    sky::SerializationContext::Get();
    sky::editor::test::RegisterTestTypes();
    sky::editor::RegisterEditorSettings(sky::SerializationContext::Get());
    sky::editor::ViewportControlSettings::Reflect();
    sky::editor::refldemo::RegisterTypes(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}
