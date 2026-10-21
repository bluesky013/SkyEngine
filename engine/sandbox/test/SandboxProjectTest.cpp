#include <editor/sandbox/SandboxModule.h>

#include <cstdlib>
#include <filesystem>
#include <framework/asset/AssetDataBase.h>
#include <framework/platform/PlatformBase.h>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

namespace sky::editor {
    class SandboxProjectTestAccess {
    public:
        static void Bind(SandboxModule &module, ProjectManagerView &view)
        {
            module.hubMode = true;
            module.hubView = &view;
        }
        static void Create(SandboxModule &module, const std::string &path)
        {
            FileBrowserResult result;
            result.accepted  = true;
            result.directory = true;
            result.path      = path;
            module.OnFileBrowserResult(result);
        }
        static void
        Start(SandboxModule &module, sky::ProcessDesc desc, const std::string &path, std::chrono::milliseconds timeout = std::chrono::seconds(5))
        {
            module.StartProjectCreation(std::move(desc), path, timeout);
        }
        static void Poll(SandboxModule &module)
        {
            module.PollProjectCreation();
        }
        static bool Pending(const SandboxModule &module)
        {
            return module.projectCreation.valid();
        }
        static void Open(SandboxModule &module, const std::string &path)
        {
            module.OpenProject(path);
        }
        static bool HasHub(const SandboxModule &module, const ProjectManagerView &view)
        {
            return module.hubMode && module.hubView == &view && !module.shell.IsBuilt();
        }
        static bool Locked(const SandboxModule &module)
        {
            return module.projectLock.IsLocked();
        }
    };
} // namespace sky::editor

namespace {
    namespace fs = std::filesystem;
    using namespace sky::editor;
    using namespace std::chrono_literals;

    void SetEnvironment(const char *name, const char *value)
    {
#if defined(_WIN32)
        _putenv_s(name, value == nullptr ? "" : value);
#else
        if (value) {
            setenv(name, value, 1);
        } else {
            unsetenv(name);
        }
#endif
    }

    struct ScopedEnvironment {
        std::string name;
        std::string previous;
        bool        existed;
        ScopedEnvironment(const char *key, const char *value) : name(key), existed(std::getenv(key) != nullptr)
        {
            if (existed) {
                previous = std::getenv(key);
            }
            SetEnvironment(key, value);
        }
        ~ScopedEnvironment()
        {
            SetEnvironment(name.c_str(), existed ? previous.c_str() : nullptr);
        }
    };

    class SandboxProjectTest : public testing::Test {
    protected:
        void SetUp() override
        {
            previousCwd = fs::current_path();
            root = fs::temp_directory_path() / ("sky-sandbox-project-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            ASSERT_TRUE(fs::create_directory(root));
            // An uninitialized Platform keeps the recent-project registry inside this isolated cwd.
            fs::current_path(root);
            sky::ProjectRegistry::Get()->Load();
            module = std::make_unique<SandboxModule>();
            view.SetBounds(sky::ui::UIRect{0, 0, 1200, 800});
            SandboxProjectTestAccess::Bind(*module, view);
        }
        void TearDown() override
        {
            module.reset();
            fs::current_path(previousCwd);
            if (!keepOutput && root.parent_path() == fs::temp_directory_path() && root.filename().string().find("sky-sandbox-project-") == 0) {
                fs::remove_all(root);
            }
        }
        sky::ProcessDesc Python(const std::string &code)
        {
            sky::ProcessDesc desc;
            desc.args = {SKY_TEST_PYTHON, "-u", "-X", "utf8", "-c", code};
            return desc;
        }
        bool Finish()
        {
            const auto deadline = std::chrono::steady_clock::now() + 8s;
            while (SandboxProjectTestAccess::Pending(*module) && std::chrono::steady_clock::now() < deadline) {
                SandboxProjectTestAccess::Poll(*module);
                std::this_thread::sleep_for(5ms);
            }
            return !SandboxProjectTestAccess::Pending(*module);
        }
        fs::path                       root;
        fs::path                       previousCwd;
        ProjectManagerView             view;
        std::unique_ptr<SandboxModule> module;
        bool                           keepOutput = false;
        ScopedEnvironment              engineRoot{"SKY_ENGINE_ROOT", SKY_TEST_ENGINE_ROOT};
        ScopedEnvironment              python{"SKY_PYTHON", SKY_TEST_PYTHON};
    };

#if defined(_WIN32)
    TEST_F(SandboxProjectTest, ScanFailureKeepsHubAndReleasesProjectLockForRetry)
    {
        ASSERT_TRUE(sky::Platform::Get()->Init({}));
        struct PlatformScope {
            ~PlatformScope()
            {
                sky::Platform::Get()->Shutdown();
            }
        } platformScope;
        const auto             path = root / "ScanFailure.skyproj";
        sky::ProjectDescriptor descriptor;
        descriptor.id            = "scan-failure";
        descriptor.name          = "ScanFailure";
        descriptor.engineVersion = sky::kEngineVersion;
        ASSERT_TRUE(descriptor.Write(path.string()));
        std::ofstream(root / "assets") << "not a directory";
        for (int attempt = 0; attempt < 2; ++attempt) {
            SandboxProjectTestAccess::Open(*module, path.string());
            EXPECT_TRUE(SandboxProjectTestAccess::HasHub(*module, view));
            EXPECT_FALSE(SandboxProjectTestAccess::Locked(*module));
            EXPECT_NE(view.GetStatus().find("Failed to scan assets at "), std::string::npos);
            EXPECT_NE(view.GetStatus().find((root / "assets").string()), std::string::npos);
            std::error_code error;
            EXPECT_FALSE(fs::exists(root / "cache" / "editor.lock", error));
            EXPECT_FALSE(error);
            EXPECT_TRUE(sky::AssetDataBase::Get()->GetSources().empty());
            EXPECT_TRUE(sky::AssetDataBase::Get()->GetMounts().empty());
            EXPECT_TRUE(sky::ProjectRegistry::Get()->Recent().empty());
        }
    }
#endif

    TEST_F(SandboxProjectTest, SlowChildReportsProgressWithoutBlockingAndGuardsHubActions)
    {
        const auto started = std::chrono::steady_clock::now();
        SandboxProjectTestAccess::Start(*module,
                                        Python("import sys,time; sys.stdout.write('[sky-cre'); sys.stdout.flush(); time.sleep(.05); "
                                               "print('ate] templates',flush=True); time.sleep(3)"),
                                        "missing.skyproj");
        EXPECT_LT(std::chrono::steady_clock::now() - started, 500ms);
        ASSERT_TRUE(view.IsBusy());
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (view.GetStatus().find("copying game template") == std::string::npos && std::chrono::steady_clock::now() < deadline) {
            SandboxProjectTestAccess::Poll(*module);
            std::this_thread::sleep_for(5ms);
        }
        EXPECT_NE(view.GetStatus().find("copying game template"), std::string::npos);
        while (view.GetStatus().find("(1s)") == std::string::npos && std::chrono::steady_clock::now() < deadline) {
            SandboxProjectTestAccess::Poll(*module);
            std::this_thread::sleep_for(5ms);
        }
        EXPECT_NE(view.GetStatus().find("(1s)"), std::string::npos);
        EXPECT_TRUE(SandboxProjectTestAccess::Pending(*module));
        const auto status = view.GetStatus();
        SandboxProjectTestAccess::Create(*module, (root / "Duplicate").string());
        EXPECT_EQ(view.GetStatus(), status);
        EXPECT_FALSE(fs::exists(root / "Duplicate"));
        int newCalls  = 0;
        int quitCalls = 0;
        view.onNew    = [&] { ++newCalls; };
        view.onQuit   = [&] { ++quitCalls; };
        for (int button : {1, 2}) {
            const auto              rect = view.TopButtonRect(button);
            sky::ui::UIPointerEvent event;
            event.action = sky::ui::UIPointerAction::DOWN;
            event.x      = (rect.left + rect.right) * 0.5f;
            event.y      = (rect.top + rect.bottom) * 0.5f;
            view.OnPointerEvent(event);
        }
        EXPECT_EQ(newCalls, 0);
        EXPECT_EQ(quitCalls, 1);
        const auto shutdownStarted = std::chrono::steady_clock::now();
        module->Shutdown();
        EXPECT_LT(std::chrono::steady_clock::now() - shutdownStarted, 2s);
        EXPECT_FALSE(SandboxProjectTestAccess::Pending(*module));
        EXPECT_TRUE(sky::ProjectRegistry::Get()->Recent().empty());
    }

    TEST_F(SandboxProjectTest, TimeoutRestoresActionsWithoutRegisteringProject)
    {
        SandboxProjectTestAccess::Start(*module, Python("import time; time.sleep(10)"), "missing.skyproj", 100ms);
        ASSERT_TRUE(Finish());
        EXPECT_FALSE(view.IsBusy());
        EXPECT_NE(view.GetStatus().find("timed out"), std::string::npos);
        EXPECT_TRUE(sky::ProjectRegistry::Get()->Recent().empty());
    }

    TEST_F(SandboxProjectTest, MissingPythonRestoresActions)
    {
        sky::ProcessDesc desc;
        desc.args = {(root / "missing-python-executable").string()};
        SandboxProjectTestAccess::Start(*module, std::move(desc), "missing.skyproj");
        ASSERT_TRUE(Finish());
        EXPECT_FALSE(view.IsBusy());
        EXPECT_NE(view.GetStatus().find("Cannot start Python"), std::string::npos);
        EXPECT_TRUE(sky::ProjectRegistry::Get()->Recent().empty());
    }

    TEST_F(SandboxProjectTest, DrainsBothPipesAndReportsNonzeroExit)
    {
        SandboxProjectTestAccess::Start(*module,
                                        Python("import sys; sys.stdout.write('o'*65536); sys.stdout.flush(); "
                                               "sys.stderr.write('e'*65536); sys.stderr.write('failure-sentinel'); sys.exit(7)"),
                                        "missing.skyproj");
        ASSERT_TRUE(Finish());
        EXPECT_FALSE(view.IsBusy());
        EXPECT_NE(view.GetStatus().find("exit 7"), std::string::npos);
        EXPECT_NE(view.GetStatus().find("failure-sentinel"), std::string::npos);
        EXPECT_TRUE(sky::ProjectRegistry::Get()->Recent().empty());
    }

    TEST_F(SandboxProjectTest, ZeroExitRequiresValidDescriptor)
    {
        for (const char *contents : {"", "broken-json", "{}"}) {
            const auto path = root / "Invalid.skyproj";
            if (*contents) {
                std::ofstream(path) << contents;
            }
            SandboxProjectTestAccess::Start(*module, Python("pass"), path.string());
            ASSERT_TRUE(Finish());
            EXPECT_FALSE(view.IsBusy());
            EXPECT_NE(view.GetStatus().find("missing or invalid"), std::string::npos);
            EXPECT_TRUE(sky::ProjectRegistry::Get()->Recent().empty());
        }
    }

    TEST_F(SandboxProjectTest, RealCreationInNonAsciiDirectory)
    {
        const auto target = root / std::filesystem::path(u8"\u4e34\u65f6\u5de5\u7a0b") / "UnicodePath";
        const auto path   = target / "UnicodePath.skyproj";
        SandboxProjectTestAccess::Create(*module, target.string());
        ASSERT_TRUE(Finish());
        EXPECT_FALSE(view.IsBusy());
        EXPECT_EQ(view.GetStatus(), "Created UnicodePath - select it and click Open");
        EXPECT_TRUE(fs::is_regular_file(path));
        ASSERT_EQ(sky::ProjectRegistry::Get()->Recent().size(), 1u);
        EXPECT_EQ(sky::ProjectRegistry::Get()->Recent().front(), path.string());
    }

    TEST_F(SandboxProjectTest, RealCreationWithSpacesAndShellCharacters)
    {
        const std::string name   = "Sandbox Verify & Space";
        const auto        target = root / name;
        const auto        path   = target / (name + ".skyproj");
        SandboxProjectTestAccess::Create(*module, target.string());
        ASSERT_TRUE(view.IsBusy());
        ASSERT_TRUE(Finish()) << view.GetStatus();
        EXPECT_FALSE(view.IsBusy());
        ASSERT_EQ(view.GetStatus(), "Created " + name + " - select it and click Open");
        sky::ProjectDescriptor descriptor;
        ASSERT_TRUE(descriptor.Read(path.string()));
        EXPECT_EQ(descriptor.name, name);
        ASSERT_EQ(descriptor.modules.size(), 1u);
        for (const char *dir : {"assets", "configs", "cache", "native/android", "native/ios", "native/macos", "native/windows"}) {
            EXPECT_TRUE(fs::is_directory(target / dir)) << dir;
        }
        for (const char *file : {"native/CMakeLists.txt", "native/CMakePresets.json", "native/source/CMakeLists.txt", "native/windows/main.cpp",
                                 "configs/modules_game.json", "export_presets.json"}) {
            EXPECT_TRUE(fs::is_regular_file(target / file)) << file;
        }
        EXPECT_TRUE(fs::is_regular_file(target / "native/source/src" / (descriptor.modules.front() + ".cpp")));
        sky::ProjectRegistry::Get()->Load();
        ASSERT_EQ(sky::ProjectRegistry::Get()->Recent().size(), 1u);
        EXPECT_EQ(sky::ProjectRegistry::Get()->Recent().front(), path.string());
        SandboxProjectTestAccess::Create(*module, target.string());
        EXPECT_FALSE(SandboxProjectTestAccess::Pending(*module));
        EXPECT_NE(view.GetStatus().find("Target already exists"), std::string::npos);
        keepOutput = std::getenv("SKY_KEEP_PROJECT_TEST_OUTPUT") != nullptr;
        if (keepOutput) {
            std::cout << "Verified project: " << path.string() << std::endl;
        }
    }
} // namespace
