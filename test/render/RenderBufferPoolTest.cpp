//
// Created by Zach Lee on 2022/2/1.
//

#include <gtest/gtest.h>
#include <render/DriverManager.h>
#include <render/MaterialDatabase.h>

using namespace sky;

struct TestData {
    int    a = 0;
    float  b = 1.f;
    double c = 2.0;
};

TEST(RenderTest, RenderBufferPoolTest)
{
    DriverManager::Get()->Initialize({});

    uint32_t count = std::min(MAX_MATERIAL_COUNT_PER_BLOCK, static_cast<uint32_t>(DEFAULT_MATERIAL_BUFFER_BLOCK / sizeof(TestData)));

    {
        std::unique_ptr<MaterialDatabase> database = std::make_unique<MaterialDatabase>(2);

        auto            view = database->Allocate(1);
        const TestData *ptr  = reinterpret_cast<const TestData *>(view->GetBuffer()->Data());

        TestData t = {};
        ASSERT_EQ(view->GetDynamicOffset(), 0);

        view->Write(t);
        ASSERT_EQ(ptr[0].a, t.a);
        ASSERT_EQ(ptr[0].b, t.b);
        ASSERT_EQ(ptr[0].c, t.c);

        view->RequestUpdate();
        ASSERT_EQ(view->GetDynamicOffset(), count * sizeof(TestData));

        t.a = 4;
        t.b = 5.f;
        t.c = 6.0;
        view->Write(t);
        ASSERT_EQ(ptr[count].a, t.a);
        ASSERT_EQ(ptr[count].b, t.b);
        ASSERT_EQ(ptr[count].c, t.c);

        view->RequestUpdate();
        ASSERT_EQ(view->GetDynamicOffset(), 0);

        t.a = 7;
        t.b = 8.f;
        t.c = 9.0;
        view->Write(t);
        ASSERT_EQ(ptr[0].a, t.a);
        ASSERT_EQ(ptr[0].b, t.b);
        ASSERT_EQ(ptr[0].c, t.c);
    }

    DriverManager::Get()->Destroy();
}