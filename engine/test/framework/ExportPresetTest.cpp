//
// ExportPreset: JSON round-trip + reflection registration (editor preset UI data).
//

#include <framework/project/ExportPreset.h>
#include <framework/serialization/SerializationContext.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

TEST(ExportPresetTest, JsonRoundTrip)
{
    const auto root = std::filesystem::temp_directory_path() / "sky_export_preset_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto path = (root / "export_presets.json").string();

    sky::ExportPresets presets;
    sky::ExportPreset android;
    android.name        = "Game-android";
    android.platform    = "android";
    android.variant     = "release";
    android.bundleId    = "com.acme.game";
    android.versionName = "2.3";
    android.versionCode = 7;
    android.keystore    = "release";
    android.abis        = {"arm64-v8a"};
    android.bundles     = {"common", "tex_low"};
    presets.presets.push_back(android);

    sky::ExportPreset macos;
    macos.name    = "Game-macos";
    macos.platform = "macos";
    macos.bundle  = true;
    macos.signing = "off";
    presets.presets.push_back(macos);

    ASSERT_TRUE(presets.Save(path));

    sky::ExportPresets loaded;
    ASSERT_TRUE(loaded.Load(path));
    ASSERT_EQ(loaded.presets.size(), 2u);

    const sky::ExportPreset *a = loaded.Find("Game-android");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->bundleId, "com.acme.game");
    EXPECT_EQ(a->versionCode, 7);
    EXPECT_EQ(a->bundles.size(), 2u);
    EXPECT_EQ(a->bundles[1], "tex_low");
    EXPECT_EQ(a->abis.size(), 1u);

    const sky::ExportPreset *m = loaded.Find("Game-macos");
    ASSERT_NE(m, nullptr);
    EXPECT_TRUE(m->bundle);
    EXPECT_EQ(m->signing, "off");

    std::filesystem::remove_all(root);
}

TEST(ExportPresetTest, ReflectionRegistered)
{
    sky::RegisterExportPresetReflection();
    const sky::TypeNode *node = sky::GetTypeNode(sky::TypeInfo<sky::ExportPreset>::RegisteredId());
    EXPECT_NE(node, nullptr);
    const sky::TypeNode *document = sky::GetTypeNode(sky::TypeInfo<sky::ExportPresets>::RegisteredId());
    EXPECT_NE(document, nullptr);
}

// Guards the shared schema: the reader/writer must accept exactly the keys the
// packaging CLI (python/package_project.py) reads from export_presets.json.
TEST(ExportPresetTest, CliSchemaKeys)
{
    const auto root = std::filesystem::temp_directory_path() / "sky_export_preset_cli_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto inPath  = (root / "in.json").string();
    const auto outPath = (root / "out.json").string();

    // Fixture shaped exactly like the CLI-written / template file.
    {
        std::ofstream out(inPath);
        out << R"([ { "name": "Game-android", "platform": "android", "variant": "release", )"
            << R"("bundleId": "com.acme.game", "versionName": "2.3", "versionCode": 7, )"
            << R"("signing": "off", "bundle": true, "team": "TEAM1", "signingIdentity": "id", )"
            << R"("provisioningProfile": "prof", "bundles": ["common","tex_low"], "abis": ["arm64-v8a"], )"
            << R"("keystore": "release", "compileSdk": 34, "minSdk": 29, "targetSdk": 33, "ndkVersion": "28.1" } ])" << "\n";
    }

    sky::ExportPresets loaded;
    ASSERT_TRUE(loaded.Load(inPath));
    ASSERT_EQ(loaded.presets.size(), 1u);
    const sky::ExportPreset &p = loaded.presets.front();
    EXPECT_EQ(p.name, "Game-android");
    EXPECT_EQ(p.platform, "android");
    EXPECT_EQ(p.variant, "release");
    EXPECT_EQ(p.bundleId, "com.acme.game");
    EXPECT_EQ(p.versionName, "2.3");
    EXPECT_EQ(p.versionCode, 7);
    EXPECT_EQ(p.signing, "off");
    EXPECT_TRUE(p.bundle);
    EXPECT_EQ(p.team, "TEAM1");
    EXPECT_EQ(p.signingIdentity, "id");
    EXPECT_EQ(p.provisioningProfile, "prof");
    EXPECT_EQ(p.bundles.size(), 2u);
    EXPECT_EQ(p.abis.size(), 1u);
    EXPECT_EQ(p.keystore, "release");
    EXPECT_EQ(p.compileSdk, 34);
    EXPECT_EQ(p.minSdk, 29);
    EXPECT_EQ(p.targetSdk, 33);
    EXPECT_EQ(p.ndkVersion, "28.1");

    // Round-trip: what the editor saves must carry the same keys for the CLI.
    ASSERT_TRUE(loaded.Save(outPath));
    std::ifstream  in(outPath);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (const char *key : {"platform", "variant", "bundleId", "versionName", "versionCode", "signing",
                            "bundle", "team", "signingIdentity", "provisioningProfile", "bundles", "abis",
                            "keystore", "compileSdk", "minSdk", "targetSdk", "ndkVersion"}) {
        EXPECT_NE(text.find(std::string("\"") + key + "\""), std::string::npos) << key;
    }

    std::filesystem::remove_all(root);
}
