//
// Created by Zach Lee on 2021/12/4.
//

#include <gtest/gtest.h>
#include <framework/asset/AssetManager.h>

using namespace sky;

TEST(AssetTest, AssetManagerSingleton)
{
    auto mgr1 = AssetManager::Get();
    ASSERT_NE(mgr1, nullptr);

    AssetManager::Destroy();
}