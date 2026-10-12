//
// Created by blues on 2026/10/2.
//

#include <gtest/gtest.h>

#include <framework/asset/CookConfig.h>

using namespace sky;

TEST(CookConfigTest, ProjectConfigAndResolution)
{
    CookConfig config;

    const std::string json = R"({
        // comments are allowed (JSONC)
        "platforms": { "windows": "pc_bc", "ios": "mobile_astc" },
        "targets": {
            "pc_bc": { "bundle": "tex_pc", "texture": { "format": "bc7", "mips": true } },
            "mobile_astc": { "bundle": "tex_mobile" }
        }
    })";

    ASSERT_TRUE(config.Parse(json));

    EXPECT_EQ(config.ResolveTargetForPlatform("windows"), "pc_bc");
    EXPECT_EQ(config.ResolveTargetForPlatform("ios"), "mobile_astc");
    EXPECT_EQ(config.ResolveTargetForPlatform("linux"), "");

    const auto *pc = config.FindTarget("pc_bc");
    ASSERT_NE(pc, nullptr);
    EXPECT_EQ(pc->bundle, "tex_pc");

    // Asset-level override wins.
    EXPECT_EQ(config.ResolveTarget(R"({"targets":["mobile_astc"]})"), "mobile_astc");

    // No preset/override -> primary bundle.
    EXPECT_EQ(config.ResolveTarget(""), "common");

    // Active platform preset applies when the asset has no override.
    config.SetActivePlatform("windows");
    EXPECT_EQ(config.ResolveTarget(R"({})"), "pc_bc");
}

TEST(CookConfigTest, BundlesAndPresets)
{
    CookConfig config;

    const std::string json = R"({
        "bundles": ["common", "tex_pc", "tex_mobile", "tex_low", "tex_hd"],
        "presets": { "windows": ["common", "tex_pc"], "ios": ["common", "tex_mobile"] },
        "distributions": { "game-normal": ["common", "tex_low"], "game-hd": ["common", "tex_hd"] }
    })";

    ASSERT_TRUE(config.Parse(json));
    EXPECT_EQ(config.GetBundles().size(), 5U);

    const auto win = config.GetPresetBundles("windows");
    ASSERT_EQ(win.size(), 2U);
    EXPECT_EQ(win[1], "tex_pc");

    EXPECT_TRUE(config.GetPresetBundles("linux").empty());

    // Two distributions on the same platform pick different bundles (normal vs HD).
    const auto normal = config.GetDistributionBundles("game-normal");
    ASSERT_EQ(normal.size(), 2U);
    EXPECT_EQ(normal[1], "tex_low");
    const auto hd = config.GetDistributionBundles("game-hd");
    ASSERT_EQ(hd.size(), 2U);
    EXPECT_EQ(hd[1], "tex_hd");
    EXPECT_TRUE(config.GetDistributionBundles("unknown").empty());
}

TEST(CookConfigTest, AssetTargetSettingsOverride)
{
    CookConfig config;

    // Sparse per-target overrides; non-scalar values and unknown targets are ignored.
    const std::string cook = R"({
        "targets": ["common", "tex_mobile"],
        "settings": {
            "tex_mobile": { "encode": "ASTC", "block": 4, "maxSize": 512, "generateMip": false, "extra": { "nested": 1 } },
            "tex_pc": { "quality": "SLOW" }
        }
    })";

    const auto targets = config.GetTargets(cook);
    ASSERT_EQ(targets.size(), 2u);

    const auto mobile = config.GetTargetSettings(cook, "tex_mobile");
    EXPECT_EQ(mobile.at("encode"), "ASTC");
    EXPECT_EQ(mobile.at("block"), "4");
    EXPECT_EQ(mobile.at("maxSize"), "512");
    EXPECT_EQ(mobile.at("generateMip"), "false");
    EXPECT_EQ(mobile.count("extra"), 0u); // non-scalar ignored

    const auto pc = config.GetTargetSettings(cook, "tex_pc");
    EXPECT_EQ(pc.at("quality"), "SLOW");

    EXPECT_TRUE(config.GetTargetSettings(cook, "unknown").empty());
    EXPECT_TRUE(config.GetTargetSettings("", "tex_mobile").empty());
}
