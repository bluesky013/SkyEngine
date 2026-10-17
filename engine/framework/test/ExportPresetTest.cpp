//
// ExportPreset: JSON round-trip + reflection registration (editor preset UI data).
//

#include <framework/project/ExportPreset.h>
#include <framework/serialization/SerializationContext.h>

#include <core/type/TypeInfo.h>

#include <rapidjson/document.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

TEST(ExportPresetTest, JsonRoundTrip)
{
    const auto root = std::filesystem::temp_directory_path() / "sky_export_preset_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto path = (root / "export_presets.json").string();

    sky::ExportPresets presets;
    sky::ExportPreset  android;
    android.name               = "Game-android";
    android.platform           = "android";
    android.variant            = "release";
    android.bundleId           = "com.acme.game";
    android.versionName        = "2.3";
    android.versionCode        = 7;
    android.keystore           = "release";
    android.abis               = {"arm64-v8a"};
    android.bundles            = {"common", "tex_low"};
    android.cacheVariables     = {{"CMAKE_BUILD_TYPE", "Release"}, {"STRIP", "ON"}};
    android.compileDefinitions = {"FOO=1"};
    android.compileFlags       = {"-Wall"};
    android.extraConfigureArgs = {"-DOPT=1"};
    android.extraBuildArgs     = {"-j8"};
    presets.presets.push_back(android);

    sky::ExportPreset macos;
    macos.name     = "Game-macos";
    macos.platform = "macos";
    macos.bundle   = true;
    macos.signing  = "off";
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
    ASSERT_EQ(a->cacheVariables.size(), 2u);
    EXPECT_EQ(a->cacheVariables[0].key, "CMAKE_BUILD_TYPE");
    EXPECT_EQ(a->cacheVariables[0].value, "Release");
    EXPECT_EQ(a->cacheVariables[1].key, "STRIP");
    ASSERT_EQ(a->compileDefinitions.size(), 1u);
    EXPECT_EQ(a->compileDefinitions[0], "FOO=1");
    ASSERT_EQ(a->compileFlags.size(), 1u);
    EXPECT_EQ(a->compileFlags[0], "-Wall");
    ASSERT_EQ(a->extraConfigureArgs.size(), 1u);
    EXPECT_EQ(a->extraConfigureArgs[0], "-DOPT=1");
    ASSERT_EQ(a->extraBuildArgs.size(), 1u);
    EXPECT_EQ(a->extraBuildArgs[0], "-j8");

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

// Guards the shared schema against drift: the reflected member set (the single
// source of the export preset field keys) must equal the manifest that the
// packaging CLI (python/package_project.py) is checked against.
TEST(ExportPresetTest, CliSchemaKeys)
{
    std::ifstream manifest(SKY_EXPORT_PRESET_SCHEMA);
    ASSERT_TRUE(manifest) << "manifest not found: " << SKY_EXPORT_PRESET_SCHEMA;
    const std::string   manifestText((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
    rapidjson::Document manifestDoc;
    manifestDoc.Parse(manifestText.c_str());
    ASSERT_FALSE(manifestDoc.HasParseError());
    ASSERT_TRUE(manifestDoc.IsObject() && manifestDoc.HasMember("keys") && manifestDoc["keys"].IsArray());
    std::set<std::string> manifestKeys;
    for (const auto &key : manifestDoc["keys"].GetArray()) {
        ASSERT_TRUE(key.IsString());
        manifestKeys.insert(key.GetString());
    }
    ASSERT_FALSE(manifestKeys.empty());

    sky::RegisterExportPresetReflection();
    const sky::TypeNode *node = sky::GetTypeNode(sky::TypeInfo<sky::ExportPreset>::RegisteredId());
    ASSERT_NE(node, nullptr);
    std::set<std::string> reflectionKeys;
    for (const auto &member : node->members) {
        reflectionKeys.insert(std::string(member.first));
    }
    EXPECT_EQ(reflectionKeys, manifestKeys);
}

// Loads a fixture shaped exactly like the CLI-written / template file (including
// the build escape hatches), saves it again, and confirms every value survives
// and `cacheVariables` stays a JSON object (the CLI reads it as a dict).
TEST(ExportPresetTest, CliFixtureRoundTrip)
{
    const auto root = std::filesystem::temp_directory_path() / "sky_export_preset_cli_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto inPath  = (root / "in.json").string();
    const auto outPath = (root / "out.json").string();

    {
        std::ofstream out(inPath);
        out << R"([ { "name": "Game-android", "platform": "android", "variant": "release", )"
            << R"("bundleId": "com.acme.game", "versionName": "2.3", "versionCode": 7, )"
            << R"("signing": "off", "bundle": true, "team": "TEAM1", "signingIdentity": "id", )"
            << R"("provisioningProfile": "prof", "bundles": ["common","tex_low"], "abis": ["arm64-v8a"], )"
            << R"("keystore": "release", "compileSdk": 34, "minSdk": 29, "targetSdk": 33, "ndkVersion": "28.1", )"
            << R"("cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "STRIP": "ON" }, )"
            << R"("compileDefinitions": ["FOO=1"], "compileFlags": ["-Wall"], "extraConfigureArgs": ["-DOPT=1"], )"
            << R"("extraBuildArgs": ["-j8"] } ])"
            << "\n";
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
    ASSERT_EQ(p.cacheVariables.size(), 2u);
    EXPECT_EQ(p.cacheVariables[0].key, "CMAKE_BUILD_TYPE");
    EXPECT_EQ(p.cacheVariables[0].value, "Release");
    EXPECT_EQ(p.cacheVariables[1].key, "STRIP");
    EXPECT_EQ(p.cacheVariables[1].value, "ON");
    ASSERT_EQ(p.compileDefinitions.size(), 1u);
    EXPECT_EQ(p.compileDefinitions[0], "FOO=1");
    ASSERT_EQ(p.compileFlags.size(), 1u);
    EXPECT_EQ(p.compileFlags[0], "-Wall");
    ASSERT_EQ(p.extraConfigureArgs.size(), 1u);
    EXPECT_EQ(p.extraConfigureArgs[0], "-DOPT=1");
    ASSERT_EQ(p.extraBuildArgs.size(), 1u);
    EXPECT_EQ(p.extraBuildArgs[0], "-j8");

    ASSERT_TRUE(loaded.Save(outPath));
    {
        std::ifstream       in(outPath);
        const std::string   text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        rapidjson::Document savedDoc;
        savedDoc.Parse(text.c_str());
        ASSERT_FALSE(savedDoc.HasParseError());
        ASSERT_TRUE(savedDoc.IsArray() && savedDoc.Size() == 1u);
        ASSERT_TRUE(savedDoc[0].IsObject());
        EXPECT_TRUE(savedDoc[0].HasMember("cacheVariables"));
        EXPECT_TRUE(savedDoc[0]["cacheVariables"].IsObject());
    }

    sky::ExportPresets reloaded;
    ASSERT_TRUE(reloaded.Load(outPath));
    ASSERT_EQ(reloaded.presets.size(), 1u);
    ASSERT_EQ(reloaded.presets.front().cacheVariables.size(), 2u);
    EXPECT_EQ(reloaded.presets.front().cacheVariables[1].value, "ON");

    std::filesystem::remove_all(root);
}
