//
// ProjectDescriptor runtime settings serialization.
//

#include <framework/project/ProjectDescriptor.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST(ProjectDescriptorTest, SettingsRoundTrip)
{
    const auto root = std::filesystem::temp_directory_path() / "sky_descriptor_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto path = (root / "Test.skyproj").string();

    sky::ProjectDescriptor source;
    source.id              = "abc";
    source.name            = "Test";
    source.engineVersion   = sky::kEngineVersion;
    source.versionName     = "3.4";
    source.versionCode     = 12;
    source.resourceModules = {"modules/game/resources.json"};
    ASSERT_TRUE(source.Write(path));

    sky::ProjectDescriptor loaded;
    ASSERT_TRUE(loaded.Read(path));
    EXPECT_EQ(loaded.versionName, "3.4");
    EXPECT_EQ(loaded.versionCode, 12);
    EXPECT_EQ(loaded.resourceModules, source.resourceModules);

    std::filesystem::remove_all(root);
}

TEST(ProjectDescriptorTest, LegacyWithoutSettingsStillReads)
{
    const auto root = std::filesystem::temp_directory_path() / "sky_descriptor_legacy_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto path = (root / "Old.skyproj").string();

    {
        std::ofstream out(path);
        out << "{\n \"id\": \"x\", \"name\": \"Old\", \"engineVersion\": \"0.1.0\",\n"
            << " \"settings\": { \"rhi\": \"metal\" },\n \"modules\": [\"M\"], \"plugins\": []\n}\n";
    }

    sky::ProjectDescriptor loaded;
    ASSERT_TRUE(loaded.Read(path));
    EXPECT_EQ(loaded.name, "Old");
    EXPECT_EQ(loaded.rhi, "metal");

    std::filesystem::remove_all(root);
}

TEST(ProjectDescriptorTest, LegacyInstallIdentityIsNotWrittenBack)
{
    const auto root = std::filesystem::temp_directory_path() / "sky_descriptor_identity_test";
    std::filesystem::create_directories(root);
    const auto path = (root / "Old.skyproj").string();
    {
        std::ofstream out(path);
        out << R"({"name":"Old","settings":{"applicationId":"com.legacy.shared"}})";
    }
    sky::ProjectDescriptor descriptor;
    ASSERT_TRUE(descriptor.Read(path));
    ASSERT_TRUE(descriptor.Write(path));
    std::ifstream     input(path);
    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(json.find("applicationId"), std::string::npos);
    input.close();
    std::filesystem::remove_all(root);
}
