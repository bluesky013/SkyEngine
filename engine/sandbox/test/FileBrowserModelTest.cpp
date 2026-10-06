//
// Created on 2026/10/06.
//

#include <editor/core/filebrowser/FileBrowserModel.h>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

using namespace sky::editor;

namespace {

    namespace fs = std::filesystem;

    struct Fixture {
        fs::path root;

        Fixture()
        {
            static int counter = 0;
            root               = fs::temp_directory_path() / ("sky_fb_test_" + std::to_string(++counter));
            std::error_code ec;
            fs::remove_all(root, ec);
            fs::create_directories(root / "sub");
            std::ofstream(root / "a.skyproj") << "x";
            std::ofstream(root / "b.txt") << "x";
        }

        ~Fixture()
        {
            std::error_code ec;
            fs::remove_all(root, ec);
        }
    };

    FileBrowserRequest MakeRequest(FileBrowserMode mode, const fs::path &dir)
    {
        FileBrowserRequest request;
        request.mode      = mode;
        request.directory = dir.string();
        request.filters   = {{"Projects", {"skyproj"}}};
        return request;
    }

} // namespace

TEST(FileBrowserModelTest, ListsDirectoriesFirstThenFilteredFiles)
{
    Fixture          fixture;
    FileBrowserModel model;
    model.SetRequest(MakeRequest(FileBrowserMode::OPEN_PROJECT, fixture.root));

    const auto &entries = model.GetEntries();
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_TRUE(entries[0].isDirectory);
    EXPECT_EQ(entries[0].name, "sub");
    EXPECT_EQ(entries[1].name, "a.skyproj");
}

TEST(FileBrowserModelTest, NavigateIntoAndParent)
{
    Fixture          fixture;
    FileBrowserModel model;
    model.SetRequest(MakeRequest(FileBrowserMode::OPEN_FILE, fixture.root));

    ASSERT_TRUE(model.NavigateTo("sub"));
    EXPECT_EQ(fs::path(model.GetLocation()).filename().string(), "sub");

    ASSERT_TRUE(model.NavigateToParent());
    EXPECT_EQ(fs::canonical(fs::path(model.GetLocation())), fs::canonical(fixture.root));
}

TEST(FileBrowserModelTest, InvalidDirectoryKeepsPrevious)
{
    Fixture          fixture;
    FileBrowserModel model;
    model.SetRequest(MakeRequest(FileBrowserMode::OPEN_FILE, fixture.root));
    const std::string before = model.GetLocation();

    EXPECT_FALSE(model.SetDirectory((fixture.root / "missing").string()));
    EXPECT_EQ(model.GetLocation(), before);
    EXPECT_FALSE(model.GetError().empty());
}

TEST(FileBrowserModelTest, ExtensionFilter)
{
    FileBrowserModel   model;
    FileBrowserRequest request;
    request.mode      = FileBrowserMode::OPEN_PROJECT;
    request.directory = ".";
    request.filters   = {{"Projects", {"skyproj"}}};
    model.SetRequest(request);

    EXPECT_TRUE(model.AcceptsFileName("x.skyproj"));
    EXPECT_TRUE(model.AcceptsFileName("x.SKYPROJ"));
    EXPECT_FALSE(model.AcceptsFileName("x.txt"));
    EXPECT_FALSE(model.AcceptsFileName(""));
}

TEST(FileBrowserModelTest, SelectDirectoryCombinesName)
{
    Fixture          fixture;
    FileBrowserModel model;
    model.SetRequest(MakeRequest(FileBrowserMode::SELECT_DIRECTORY, fixture.root));
    model.SetName("MyProject");

    EXPECT_TRUE(model.CanAccept());
    EXPECT_EQ(fs::path(model.ResultPath()), fixture.root / "MyProject");

    for (const auto &entry : model.GetEntries()) {
        EXPECT_TRUE(entry.isDirectory);
    }
}

TEST(FileBrowserModelTest, CreateFolderGeneratesUniqueNames)
{
    Fixture          fixture;
    FileBrowserModel model;
    model.SetRequest(MakeRequest(FileBrowserMode::OPEN_FILE, fixture.root));

    EXPECT_TRUE(model.CreateFolder());
    EXPECT_TRUE(fs::is_directory(fixture.root / "New Folder"));
    EXPECT_EQ(model.GetName(), "New Folder");

    EXPECT_TRUE(model.CreateFolder());
    EXPECT_TRUE(fs::is_directory(fixture.root / "New Folder 2"));

    EXPECT_TRUE(model.CreateFolder("Custom"));
    EXPECT_TRUE(fs::is_directory(fixture.root / "Custom"));
}

TEST(FileBrowserModelTest, RenameSelected)
{
    Fixture          fixture;
    FileBrowserModel model;
    model.SetRequest(MakeRequest(FileBrowserMode::OPEN_FILE, fixture.root));

    model.SetSelected(0); // "sub" sorts first
    ASSERT_NE(model.GetSelectedEntry(), nullptr);
    EXPECT_TRUE(model.RenameSelected("renamed"));

    EXPECT_TRUE(fs::is_directory(fixture.root / "renamed"));
    EXPECT_FALSE(fs::exists(fixture.root / "sub"));
    EXPECT_EQ(model.GetName(), "renamed");
}
