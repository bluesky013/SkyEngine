#include "ReflectionTestEnvironment.h"
#include <core/type/TypeInfoObj.h>
#include <editor/core/EditorCore.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/property/ReflectedForm.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/CookConfig.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace sky::editor;

namespace {
    struct HistorySettings {
        uint32_t           maxSize = 0;
        std::string        label   = "preset";
        std::vector<float> items;
    };
    struct ReplacementSettings {
        uint32_t maxSize = 0;
    };

    const auto *reflectionEnvironment = sky::editor::test::AddReflectionEnvironment([] {
        auto *context = sky::SerializationContext::Get();
        context->Register<HistorySettings>("AssetHistoryHeapSettings")
            .Member<&HistorySettings::maxSize>("maxSize")
            .Member<&HistorySettings::label>("label")
            .Member<&HistorySettings::items>("items");
        context->Register<ReplacementSettings>("AssetHistoryReplacementSettings").Member<&ReplacementSettings::maxSize>("maxSize");
    });

    class HistoryBuilder : public sky::AssetBuilder {
    public:
        HistoryBuilder()
        {
        }
        const std::vector<std::string> &GetExtensions() const override
        {
            static const std::vector<std::string> extensions{".historylife"};
            return extensions;
        }
        std::string_view QueryType(const std::string &) const override
        {
            return "HistoryLifetimeAsset";
        }
        const sky::TypeInfoRT *GetSettingsType() const override
        {
            return replacement ? sky::TypeInfoObj<ReplacementSettings>::Get()->RtInfo() : sky::TypeInfoObj<HistorySettings>::Get()->RtInfo();
        }
        sky::Any MakeSettings(const sky::ProductBundleKey &, const sky::BuildSettingsOverride &values) const override
        {
            if (replacement) {
                return sky::Any(std::in_place_type<ReplacementSettings>);
            }
            HistorySettings result;
            if (const auto it = values.find("maxSize"); it != values.end()) {
                result.maxSize = static_cast<uint32_t>(std::stoul(it->second));
            }
            if (const auto it = values.find("label"); it != values.end()) {
                result.label = it->second;
            }
            if (const auto it = values.find("items"); it != values.end()) {
                std::istringstream input(it->second);
                float              value = 0.0f;
                while (input >> value) {
                    result.items.push_back(value);
                }
            }
            return sky::Any(result);
        }
        sky::BuildSettingsOverride DiffSettings(const sky::ProductBundleKey &, const sky::Any &value) const override
        {
            sky::BuildSettingsOverride result;
            if (const auto *typed = value.GetAsConst<HistorySettings>()) {
                if (typed->maxSize != 0) {
                    result["maxSize"] = std::to_string(typed->maxSize);
                }
                if (typed->label != "preset") {
                    result["label"] = typed->label;
                }
                if (!typed->items.empty()) {
                    std::ostringstream output;
                    for (float item : typed->items) {
                        output << item << ' ';
                    }
                    result["items"] = output.str();
                }
            }
            return result;
        }
        bool replacement = false;
    };

    class AssetSettingsHistoryTest : public testing::Test {
    protected:
        void SetUp() override
        {
            static_assert(sizeof(HistorySettings) > 24, "Exercise Any heap-backed model release");
            previousConfig    = manager->GetCookConfig();
            previousWorkspace = database->GetWorkSpaceFs();
            std::filesystem::create_directories(root);
            std::ofstream(root / "a.historylife") << "a";
            std::ofstream(root / "b.historylife") << "b";
            builder = new HistoryBuilder();
            manager->RegisterBuilder(builder);
            sky::CookConfig config;
            ASSERT_TRUE(config.Parse(R"({"bundles":["common","other"],"presets":{"windows":["common","other"]}})"));
            config.SetActivePlatform("windows");
            manager->SetCookConfig(std::move(config));
            database->Reset();
            database->SetWorkSpaceFs(new sky::NativeFileSystem(sky::FilePath(root.string())));
            EXPECT_FALSE(database->RebuildCacheFromScan().has_value());
            catalog->Refresh();
            EditorAssetItem item;
            ASSERT_TRUE(catalog->FindByPath("Project/a.historylife", item));
            asset = item.uuid;
        }
        void TearDown() override
        {
            manager->SetCookConfig(std::move(previousConfig));
            manager->UnRegisterBuilder(builder);
            database->Reset();
            database->SetWorkSpaceFs(previousWorkspace);
            EXPECT_FALSE(database->RebuildCacheFromScan().has_value());
            catalog->Refresh();
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }
        uint32_t MaxSize(const std::string &target)
        {
            auto        settings = catalog->GetCookSettings(asset, target);
            const auto *typed    = settings.object.GetAsConst<HistorySettings>();
            if (typed == nullptr) {
                ADD_FAILURE() << "Missing history settings";
                return 0;
            }
            return typed->maxSize;
        }
        PropertyField *Field(ReflectedForm &form, const char *name)
        {
            for (auto &section : form.GetSections()) {
                for (auto &field : section.fields) {
                    if (field.path == name) {
                        return &field;
                    }
                }
            }
            return nullptr;
        }
        sky::AssetBuilderManager *manager  = sky::AssetBuilderManager::Get();
        sky::AssetDataBase       *database = sky::AssetDataBase::Get();
        EditorAssetCatalog       *catalog  = EditorAssetCatalog::Get();
        sky::CookConfig           previousConfig;
        sky::FileSystemPtr        previousWorkspace;
        HistoryBuilder           *builder = nullptr;
        sky::Uuid                 asset;
        std::filesystem::path     root = std::filesystem::temp_directory_path() / ("sky_asset_history_" + sky::Uuid::Create().ToString());
    };
} // namespace

TEST_F(AssetSettingsHistoryTest, UndoOwnsTargetSnapshotsAfterHeapModelReleaseAndPreservesOtherTargets)
{
    CommandService commands;
    auto           settings = catalog->GetCookSettings(asset, "common");
    ASSERT_TRUE(settings.IsValid());
    settings.object.GetAs<HistorySettings>()->maxSize = 512;
    auto command                                      = catalog->MakeCookSettingsEditCommand(asset, "common", settings.object);
    ASSERT_NE(command, nullptr);
    settings                 = {}; // stored command cannot retain this model or reflection descriptor
    int        notifications = 0;
    const auto observer      = catalog->AddObserver([&]() { ++notifications; });
    commands.Execute(std::move(command));
    EXPECT_EQ(MaxSize("common"), 512u);

    auto other                                     = catalog->GetCookSettings(asset, "other");
    other.object.GetAs<HistorySettings>()->maxSize = 77;
    ASSERT_TRUE(catalog->ApplyCookSettings(asset, "other", other.object));
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(MaxSize("common"), 0u);
    EXPECT_EQ(MaxSize("other"), 77u);
    const auto &config = manager->GetCookConfig();
    EXPECT_TRUE(config.GetTargetSettings(database->GetCookJson(asset), "common").empty());
    EXPECT_EQ(config.GetTargetSettings(database->GetCookJson(asset), "other").at("maxSize"), "77");
    ASSERT_TRUE(commands.Redo());
    EXPECT_EQ(MaxSize("common"), 512u);
    EXPECT_EQ(MaxSize("other"), 77u);
    EXPECT_EQ(notifications, 4);
    catalog->RemoveObserver(observer);

    std::ifstream     manifest(root / "assets.jsonl");
    const std::string persisted((std::istreambuf_iterator<char>(manifest)), {});
    EXPECT_NE(persisted.find("512"), std::string::npos);
    EXPECT_NE(persisted.find("77"), std::string::npos);
}

TEST_F(AssetSettingsHistoryTest, FactoryRoutesScalarResetAndSequenceOperationsThroughDomainHistory)
{
    PropertyEditorRegistry registry;
    EditorCore::RegisterDefaultPropertyEditors(registry);
    CommandService commands;
    ReflectedForm  form;
    auto           settings = catalog->GetCookSettings(asset, "common");
    form.Build({settings.object.Data(), settings.type}, registry);
    form.SetEditCommandFactory([&](const PropertyDescriptor &property, sky::Any value) {
        return catalog->MakeCookSettingsPropertyEditCommand(asset, "common", property.GetName(), std::move(value));
    });
    int changed = 0;
    form.SetOnChanged([&]() { ++changed; });
    auto reload = [&]() {
        form.Build({}, registry);
        settings = catalog->GetCookSettings(asset, "common");
        form.Build({settings.object.Data(), settings.type}, registry);
    };
    ASSERT_NE(Field(form, "maxSize"), nullptr);
    ASSERT_TRUE(form.Edit(*Field(form, "maxSize"), sky::Any(uint32_t(256)), commands));
    reload();
    ASSERT_TRUE(form.ResetToDefault(*Field(form, "maxSize"), commands));
    EXPECT_EQ(MaxSize("common"), 0u);
    reload();
    ASSERT_TRUE(form.AddSequenceElement(*Field(form, "items"), commands));
    reload();
    EXPECT_EQ(settings.object.GetAsConst<HistorySettings>()->items.size(), 1u);
    ASSERT_TRUE(form.RemoveSequenceElement(*Field(form, "items"), 0, commands));
    EXPECT_EQ(changed, 4);
    form.Build({}, registry);
    settings = {};
    ASSERT_TRUE(commands.Undo());
    auto restored = catalog->GetCookSettings(asset, "common");
    EXPECT_EQ(restored.object.GetAsConst<HistorySettings>()->items.size(), 1u);
    ASSERT_TRUE(commands.Redo());
    restored = catalog->GetCookSettings(asset, "common");
    EXPECT_TRUE(restored.object.GetAsConst<HistorySettings>()->items.empty());
}

TEST_F(AssetSettingsHistoryTest, MissingAssetRemovedTargetAndReplacementSchemaRejectStaleCommands)
{
    CommandService commands;
    auto           command = catalog->MakeCookSettingsPropertyEditCommand(asset, "common", "maxSize", sky::Any(uint32_t(512)));
    ASSERT_NE(command, nullptr);
    commands.Execute(std::move(command));
    const auto persisted = database->GetCookJson(asset);
    builder->replacement = true;
    ASSERT_TRUE(commands.Undo()); // cursor advances, while stale schema safely performs no persistence
    EXPECT_EQ(database->GetCookJson(asset), persisted);
    EXPECT_EQ(catalog->MakeCookSettingsPropertyEditCommand(asset, "common", "label", sky::Any(std::string("bad"))), nullptr);
    builder->replacement = false;
    ASSERT_TRUE(commands.Redo());

    sky::CookConfig removed;
    ASSERT_TRUE(removed.Parse(R"({"bundles":["other"],"presets":{"windows":["other"]}})"));
    removed.SetActivePlatform("windows");
    manager->SetCookConfig(std::move(removed));
    ASSERT_TRUE(commands.Undo());
    EXPECT_EQ(database->GetCookJson(asset), persisted);
    EXPECT_EQ(catalog->MakeCookSettingsPropertyEditCommand(asset, "common", "maxSize", sky::Any(uint32_t(9))), nullptr);
    database->RemoveAsset(asset);
    ASSERT_TRUE(commands.Redo());
    EXPECT_EQ(catalog->MakeCookSettingsPropertyEditCommand(asset, "other", "maxSize", sky::Any(uint32_t(9))), nullptr);
}

TEST_F(AssetSettingsHistoryTest, SynchronousObserverCanDestroyOrRebindEditingFormWithoutObsoleteCallback)
{
    PropertyEditorRegistry registry;
    EditorCore::RegisterDefaultPropertyEditors(registry);
    CommandService commands;
    auto           settings = catalog->GetCookSettings(asset, "common");
    auto           form     = std::make_unique<ReflectedForm>();
    form->Build({settings.object.Data(), settings.type}, registry);
    form->SetEditCommandFactory([&](const PropertyDescriptor &property, sky::Any value) {
        return catalog->MakeCookSettingsPropertyEditCommand(asset, "common", property.GetName(), std::move(value));
    });
    int obsoleteChanged = 0;
    form->SetOnChanged([&]() { ++obsoleteChanged; });
    auto *editing = form.get();
    auto *field   = Field(*form, "maxSize");
    ASSERT_NE(field, nullptr);
    const auto destroyObserver = catalog->AddObserver([&]() { form.reset(); });
    EXPECT_TRUE(editing->Edit(*field, sky::Any(uint32_t(512)), commands));
    EXPECT_EQ(form, nullptr);
    EXPECT_EQ(obsoleteChanged, 0);
    EXPECT_EQ(MaxSize("common"), 512u);
    catalog->RemoveObserver(destroyObserver);

    form     = std::make_unique<ReflectedForm>();
    settings = catalog->GetCookSettings(asset, "common");
    form->Build({settings.object.Data(), settings.type}, registry);
    form->SetEditCommandFactory([&](const PropertyDescriptor &property, sky::Any value) {
        return catalog->MakeCookSettingsPropertyEditCommand(asset, "common", property.GetName(), std::move(value));
    });
    form->SetOnChanged([&]() { ++obsoleteChanged; });
    const auto rebindObserver = catalog->AddObserver([&]() { form->Build({}, registry); });
    EXPECT_TRUE(form->Edit(*Field(*form, "maxSize"), sky::Any(uint32_t(256)), commands));
    EXPECT_FALSE(form->IsValid());
    EXPECT_FALSE(form->IsEditing());
    EXPECT_EQ(obsoleteChanged, 0);
    EXPECT_EQ(MaxSize("common"), 256u);
    catalog->RemoveObserver(rebindObserver);
}
