//
// Created on 2026/09/21.
//

#include <core/util/Uuid.h>
#include <editor/core/selection/SelectionService.h>
#include <gtest/gtest.h>

namespace sky::editor {

    TEST(SelectionServiceTest, SetClearAndNotify)
    {
        SelectionService service;
        int notified = 0;
        const auto id = service.AddChangedCallback([&]() { ++notified; });

        EXPECT_TRUE(service.IsEmpty());

        service.SetSelection({SelectionItem{SelectionType::ENTITY, Uuid::GetEmpty()}});
        EXPECT_EQ(notified, 1);
        EXPECT_EQ(service.GetSelection().size(), 1u);
        EXPECT_FALSE(service.IsEmpty());

        // Setting the same selection does not notify.
        service.SetSelection(service.GetSelection());
        EXPECT_EQ(notified, 1);

        service.Clear();
        EXPECT_EQ(notified, 2);
        EXPECT_TRUE(service.IsEmpty());

        // Clearing an empty selection does not notify.
        service.Clear();
        EXPECT_EQ(notified, 2);

        service.RemoveChangedCallback(id);
        service.SetSelection({SelectionItem{SelectionType::ASSET, Uuid::GetEmpty()}});
        EXPECT_EQ(notified, 2);
    }

    TEST(SelectionServiceTest, ContextChangeNotifies)
    {
        SelectionService service;
        int notified = 0;
        service.AddChangedCallback([&]() { ++notified; });

        service.SetContext("world");
        EXPECT_EQ(notified, 1);
        service.SetContext("world");
        EXPECT_EQ(notified, 1);
        service.SetContext("material");
        EXPECT_EQ(notified, 2);
        EXPECT_EQ(service.GetContext(), "material");
    }

} // namespace sky::editor
