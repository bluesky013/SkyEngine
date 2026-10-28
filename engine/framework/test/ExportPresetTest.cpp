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
    android.worlds             = {"scenes/main.world", "scenes/secondary.world"};
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
    EXPECT_EQ(a->worlds, (std::vector<std::string>{"scenes/main.world", "scenes/secondary.world"}));
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

TEST(ExportPresetTest, ShaderEnvironmentsRoundTripTypedOverridesAndCliObjectShape)
{
    const auto root = std::filesystem::temp_directory_path() / ("sky_shader_environments_" + sky::Uuid::Create().ToString());
    std::filesystem::create_directories(root);
    const auto input  = (root / "in.json").string();
    const auto output = (root / "out.json").string();
    {
        std::ofstream file(input);
        file << R"([
            {"name":"windows","platform":"windows","shaderTargets":["vulkan","d3d12"],"shaderEnvironments":{
                "vulkan":{"codeFormat":1,"profile":"spirv_1_5","minimumVersion":10500,
                          "requiredCapabilities":18446744073709551615,"matrixLayout":1,"options":["second","first"]},
                "d3d12":{"codeFormat":3,"profile":"sm_6_5","minimumVersion":60500,"options":[]}}},
            {"name":"apple","platform":"macos","shaderTargets":["metal"],"shaderEnvironments":{
                "metal":{"codeFormat":2,"profile":"metallib_2_3","minimumVersion":20300,"options":[]}}},
            {"name":"defaults","platform":"ios","shaderEnvironments":{"metal":{}}},
            {"name":"null-defaults","platform":"ios","shaderEnvironments":null}
        ])";
    }
    sky::ExportPresets presets;
    ASSERT_TRUE(presets.Load(input));
    ASSERT_EQ(presets.presets.size(), 4u);
    const auto &windows = presets.presets[0].shaderEnvironments;
    ASSERT_EQ(windows.size(), 2u);
    EXPECT_EQ(windows[0].target, "vulkan");
    EXPECT_EQ(windows[0].codeFormat, 1u);
    EXPECT_EQ(windows[0].profile, "spirv_1_5");
    EXPECT_EQ(windows[0].minimumVersion, 10500u);
    EXPECT_EQ(windows[0].requiredCapabilities, UINT64_MAX);
    EXPECT_EQ(windows[0].matrixLayout, 1u);
    EXPECT_EQ(windows[0].options, (std::vector<std::string>{"second", "first"}));
    EXPECT_EQ(windows[1].target, "d3d12");
    ASSERT_EQ(presets.presets[1].shaderEnvironments.size(), 1u);
    EXPECT_EQ(presets.presets[1].shaderEnvironments[0].profile, "metallib_2_3");
    EXPECT_EQ(presets.presets[2].shaderEnvironments[0].codeFormat, 0u);
    EXPECT_TRUE(presets.presets[3].shaderEnvironments.empty());
    ASSERT_TRUE(presets.Save(output));
    {
        std::ifstream       file(output);
        const std::string   text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        rapidjson::Document document;
        document.Parse(text.c_str());
        ASSERT_FALSE(document.HasParseError());
        ASSERT_TRUE(document.IsArray());
        ASSERT_TRUE(document[0]["shaderEnvironments"].IsObject());
        const auto &environment = document[0]["shaderEnvironments"]["vulkan"];
        EXPECT_EQ(environment["requiredCapabilities"].GetUint64(), UINT64_MAX);
        EXPECT_EQ(environment["codeFormat"].GetUint(), 1u);
        EXPECT_TRUE(environment["options"].IsArray());
        EXPECT_FALSE(environment.HasMember("target"));
        EXPECT_FALSE(environment.HasMember("classId"));
        const auto &defaults = document[2]["shaderEnvironments"]["metal"];
        EXPECT_FALSE(defaults.HasMember("codeFormat"));
        EXPECT_FALSE(defaults.HasMember("profile"));
    }
    sky::ExportPresets reloaded;
    ASSERT_TRUE(reloaded.Load(output));
    ASSERT_EQ(reloaded.presets.size(), presets.presets.size());
    for (size_t i = 0; i < presets.presets.size(); ++i)
        EXPECT_EQ(reloaded.presets[i].shaderEnvironments, presets.presets[i].shaderEnvironments);
    const auto *node = sky::GetTypeNode(sky::TypeInfo<sky::ExportPresetShaderEnvironment>::RegisteredId());
    ASSERT_TRUE(node);
    EXPECT_EQ(node->members.size(), 7u);
    EXPECT_TRUE(node->members.contains("options"));
    EXPECT_TRUE(node->members.contains("requiredCapabilities"));
    std::filesystem::remove_all(root);
}

TEST(ExportPresetTest, InvalidShaderEnvironmentsFailWithoutReplacingLoadedPresets)
{
    const auto root = std::filesystem::temp_directory_path() / ("sky_invalid_shader_environments_" + sky::Uuid::Create().ToString());
    std::filesystem::create_directories(root);
    const auto         input = (root / "in.json").string();
    sky::ExportPresets presets;
    sky::ExportPreset  original;
    original.name     = "keep";
    original.platform = "ios";
    presets.presets   = {original};
    for (const auto *invalid : {R"([])", R"({"vulkan":{}})", R"({"metal":{},"metal":{}})", R"({"metal":{"unknown":1}})",
                                R"({"metal":{"codeFormat":0}})", R"({"metal":{"codeFormat":3}})", R"({"metal":{"profile":""}})",
                                R"({"metal":{"minimumVersion":true}})", R"({"metal":{"requiredCapabilities":-1}})", R"({"metal":{"matrixLayout":2}})",
                                R"({"metal":{"options":"bad"}})", R"({"metal":{"options":[""]}})"}) {
        {
            std::ofstream file(input);
            file << "[{\"name\":\"invalid\",\"platform\":\"ios\",\"shaderEnvironments\":" << invalid << "}]";
        }
        EXPECT_FALSE(presets.Load(input)) << invalid;
        ASSERT_EQ(presets.presets.size(), 1u);
        EXPECT_EQ(presets.presets.front().name, "keep");
    }
    original.shaderEnvironments = {{.target = "metal", .matrixLayout = 2}};
    presets.presets             = {original};
    EXPECT_FALSE(presets.Save((root / "invalid.json").string()));
    EXPECT_FALSE(std::filesystem::exists(root / "invalid.json"));
    std::filesystem::remove_all(root);
}
