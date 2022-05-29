//
// Created by Zach Lee on 2022/5/29.
//


#include <gtest/gtest.h>
#include <render/RenderScene.h>

using namespace sky;

TEST(RenderSceneTest, ViewTest)
{
    auto scene = std::make_shared<RenderScene>();

    auto view1 = std::make_shared<RenderView>();
    scene->AddView(view1);

    auto view2 = std::make_shared<RenderView>();
    scene->AddView(view2);

    {
        auto& views = scene->GetViews();
        ASSERT_EQ(views.size(), 2);
    }

    {
        scene->RemoveView({});
        auto& views = scene->GetViews();
        ASSERT_EQ(views.size(), 2);
    }

    {
        scene->RemoveView(view1);
        auto& views = scene->GetViews();
        ASSERT_EQ(views.size(), 1);
    }

    {
        scene->RemoveView(view2);
        auto& views = scene->GetViews();
        ASSERT_EQ(views.size(), 0);
    }
}