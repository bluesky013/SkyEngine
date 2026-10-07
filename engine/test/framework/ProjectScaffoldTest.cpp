//
// ProjectScaffold: generates the game source tree from templates.
//

#include <framework/project/ProjectScaffold.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

    void SetTemplateDir(const char *value)
    {
#if defined(_WIN32)
        _putenv_s("SKY_TEMPLATE_DIR", value);
#else
        setenv("SKY_TEMPLATE_DIR", value, 1);
#endif
    }

    std::string ReadAll(const std::filesystem::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

} // namespace

TEST(ProjectScaffoldTest, GeneratesGameProject)
{
    // Templates are injected as an absolute path at configure time; fall back to
    // the repository-root relative path (tests normally run from there).
#ifdef SKY_TEST_TEMPLATE_DIR
    SetTemplateDir(SKY_TEST_TEMPLATE_DIR);
#else
    SetTemplateDir("engine/templates");
#endif

    const auto root = std::filesystem::temp_directory_path() / "sky_scaffold_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    ASSERT_TRUE(sky::ProjectScaffold::Scaffold(root.string(), "TestProject"));

    EXPECT_TRUE(std::filesystem::exists(root / "CMakeLists.txt"));
    EXPECT_TRUE(std::filesystem::exists(root / "CMakePresets.json"));
    EXPECT_TRUE(std::filesystem::exists(root / "source" / "CMakeLists.txt"));
    EXPECT_TRUE(std::filesystem::exists(root / "source" / "include" / "TestProjectModule.h"));
    EXPECT_TRUE(std::filesystem::exists(root / "source" / "src" / "TestProjectModule.cpp"));

    const std::string module = ReadAll(root / "source" / "src" / "TestProjectModule.cpp");
    EXPECT_NE(module.find("REGISTER_MODULE(sky::TestProjectModule)"), std::string::npos);
    EXPECT_EQ(module.find("{{PROJECT_NAME}}"), std::string::npos);

    EXPECT_EQ(sky::ProjectScaffold::ModuleName("TestProject"), "TestProjectModule");
    EXPECT_EQ(sky::ProjectScaffold::ModuleName("Test Project 1"), "TestProject1Module");

    std::filesystem::remove_all(root);
}
