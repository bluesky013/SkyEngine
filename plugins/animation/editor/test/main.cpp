#include "animation/editor/CharacterPreviewView.h"
#include "animation/editor/GraphProperties.h"
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::animation::editor::RegisterGraphProperties();
    sky::animation::editor::RegisterCharacterPreviewViewTypes();
    return RUN_ALL_TESTS();
}
