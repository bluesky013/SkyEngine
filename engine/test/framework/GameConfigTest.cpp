#include <framework/application/GameApplication.h>
#include <framework/platform/PlatformBase.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

    class GameConfigTest : public testing::Test {
    protected:
        void SetUp() override
        {
            const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
            root              = std::filesystem::temp_directory_path() / ("sky-platform-config-" + std::to_string(suffix));
            work              = new sky::NativeFileSystem(root / "work");
            bundle            = new sky::NativeFileSystem(root / "bundle");
        }

        void TearDown() override
        {
            work   = nullptr;
            bundle = nullptr;
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }

        void Write(const char *mount, const std::string &text)
        {
            const auto path = root / mount / "configs";
            std::filesystem::create_directories(path);
            std::ofstream(path / "modules_game.json") << text;
        }

        std::string ReadSelected()
        {
            const auto file = sky::OpenGameConfig(work, bundle);
            EXPECT_NE(file, nullptr);
            std::string result;
            if (file != nullptr) {
                EXPECT_TRUE(file->ReadString(result));
            }
            return result;
        }

        std::filesystem::path root;
        sky::FileSystemPtr    work;
        sky::FileSystemPtr    bundle;
    };

    class PathOnlyPlatform : public sky::PlatformBase {
    public:
        bool Init(const sky::PlatformInfo &) override
        {
            return true;
        }
        uint64_t GetPerformanceFrequency() const override
        {
            return 1000;
        }
        uint64_t GetPerformanceCounter() const override
        {
            return 1;
        }
        std::string GetInternalPath() const override
        {
            return {};
        }
        std::string GetBundlePath() const override
        {
            return bundlePath;
        }

        std::string bundlePath;
    };

} // namespace

TEST_F(GameConfigTest, FreshInstallationFallsBackToPackagedConfiguration)
{
    Write("bundle", "{\"modules\": []}");
    EXPECT_EQ(ReadSelected(), "{\"modules\": []}");
    EXPECT_FALSE(work->FileExist("configs/modules_game.json"));
    const auto file = sky::OpenGameConfig(nullptr, bundle);
    EXPECT_NE(file, nullptr);
}

TEST_F(GameConfigTest, WritableConfigurationTakesPrecedence)
{
    Write("work", "project override");
    Write("bundle", "builtin default");
    EXPECT_EQ(ReadSelected(), "project override");
}

TEST_F(GameConfigTest, InvalidOverrideIsNotHiddenByPackagedDefault)
{
    Write("work", "invalid json");
    Write("bundle", "{\"modules\": []}");
    EXPECT_EQ(ReadSelected(), "invalid json");
}

TEST_F(GameConfigTest, MissingConfigurationIsReported)
{
    EXPECT_EQ(sky::OpenGameConfig(work, bundle), nullptr);
    EXPECT_EQ(sky::OpenGameConfig(nullptr, nullptr), nullptr);
}

TEST_F(GameConfigTest, DefaultBundleFileSystemUsesNativeBundlePath)
{
    Write("bundle", "builtin default");
    PathOnlyPlatform platform;
    EXPECT_EQ(platform.GetBundleFileSystem(), nullptr);
    platform.bundlePath = (root / "bundle").string();
    const auto fs       = platform.GetBundleFileSystem();
    ASSERT_NE(fs, nullptr);
    std::string text;
    ASSERT_TRUE(fs->OpenFile("configs/modules_game.json")->ReadString(text));
    EXPECT_EQ(text, "builtin default");
}

TEST(PlatformServiceTest, UnsupportedCommandsReportFailure)
{
    PathOnlyPlatform platform;
    std::string      output;
    EXPECT_FALSE(platform.RunCmd("unsupported", output));
    EXPECT_TRUE(platform.IsFrameActive());
    platform.StopFrameLoop();
}
