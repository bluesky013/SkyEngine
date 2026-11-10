//
// Created on 2026/11/10.
//

#include <framework/application/ModuleManifest.h>

#include <algorithm>
#include <core/file/FileIO.h>
#include <filesystem>
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <set>

namespace {
    std::optional<bool> TestOption(std::string_view name)
    {
        if (name == "Enabled")
            return true;
        if (name == "Disabled")
            return false;
        return {};
    }
} // namespace

TEST(ModuleManifest, SharedConfigureAndRuntimeSelectionCases)
{
    std::string json;
    ASSERT_TRUE(sky::ReadString((std::filesystem::path(__FILE__).parent_path() / "ModuleManifestCases.json").string(), json));
    rapidjson::Document cases;
    cases.Parse(json.c_str());
    ASSERT_TRUE(cases.IsArray());
    for (const auto &entry : cases.GetArray()) {
        SCOPED_TRACE(entry["name"].GetString());
        rapidjson::StringBuffer                    buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        entry["manifest"].Accept(writer);
        auto parsed = sky::ParseModuleManifest(buffer.GetString(), TestOption);
        if (!entry.HasMember("expected")) {
            ASSERT_TRUE(std::holds_alternative<std::string>(parsed));
            EXPECT_FALSE(std::get<std::string>(parsed).empty());
            continue;
        }
        ASSERT_TRUE(std::holds_alternative<std::vector<sky::ModuleInfo>>(parsed));
        std::set<std::string> expected, actual;
        for (const auto &name : entry["expected"].GetArray())
            expected.insert(name.GetString());
        for (const auto &module : std::get<std::vector<sky::ModuleInfo>>(parsed)) {
            actual.insert(module.name);
            actual.insert(module.dependencies.begin(), module.dependencies.end());
        }
        EXPECT_EQ(actual, expected);
    }
}

TEST(ModuleManifest, RejectsMalformedAndOversizedInputs)
{
    EXPECT_TRUE(std::holds_alternative<std::string>(sky::ParseModuleManifest("{")));
    EXPECT_TRUE(std::holds_alternative<std::string>(sky::ParseModuleManifest(std::string(1024 * 1024 + 1, ' '))));
    EXPECT_TRUE(std::holds_alternative<std::string>(sky::ParseModuleManifest(R"({"modules":[]})", nullptr)));
}

TEST(ModuleManifest, DeepUnknownFieldsDoNotUseTheNativeCallStack)
{
    std::string json = R"({"modules":[],"ignored":)";
    json.append(20000, '[');
    json += '0';
    json.append(20000, ']');
    json += '}';
    const auto parsed = sky::ParseModuleManifest(json);
    ASSERT_TRUE(std::holds_alternative<std::vector<sky::ModuleInfo>>(parsed));
    EXPECT_TRUE(std::get<std::vector<sky::ModuleInfo>>(parsed).empty());
}

TEST(ModuleManifest, EditorWithoutRecastRetainsItsHostAndRequiredDependencies)
{
    std::string json;
    const auto  path = std::filesystem::path(__FILE__).parent_path() / "../../../configs/modules_editor.json";
    ASSERT_TRUE(sky::ReadString(path.string(), json));
    auto parsed = sky::ParseModuleManifest(json, [](std::string_view option) -> std::optional<bool> { return option != "SKY_BUILD_RECAST"; });
    ASSERT_TRUE(std::holds_alternative<std::vector<sky::ModuleInfo>>(parsed));
    bool hostFound = false;
    for (const auto &module : std::get<std::vector<sky::ModuleInfo>>(parsed)) {
        EXPECT_NE(module.name, "RecastNavigation.Editor");
        if (module.name == "SandboxModule") {
            hostFound = true;
            EXPECT_EQ(std::count(module.dependencies.begin(), module.dependencies.end(), "Aurora.Render"), 1);
            EXPECT_EQ(std::count(module.dependencies.begin(), module.dependencies.end(), "RecastNavigation.Editor"), 0);
        }
    }
    EXPECT_TRUE(hostFound);
}
