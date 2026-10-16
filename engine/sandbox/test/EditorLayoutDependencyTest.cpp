#include "TestTypes.h"

#include <core/environment/Once.h>
#include <core/type/TypeInfoObj.h>
#include <editor/core/EditorCore.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/document/WorldDocument.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/shell/AssetViewerWidget.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/WorldConfigPanel.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ReflectedFormView.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/CookConfig.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/world/WorldDesc.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>
#include <ui/widgets/VBox.h>

#include <filesystem>
#include <fstream>

using namespace sky::editor;

namespace {

    constexpr const char *kSubsystemName = "LayoutDependencyConfig";

    class CountingWorldConfigPanel : public WorldConfigPanel {
    public:
        using WorldConfigPanel::WorldConfigPanel;
        int arrangements = 0;

    protected:
        void ArrangeChildren(const sky::ui::UIRect &content) override
        {
            ++arrangements;
            WorldConfigPanel::ArrangeChildren(content);
        }
    };

    struct ScopedDefaultTheme {
        UiTheme saved = GetDefaultUiTheme();

        ~ScopedDefaultTheme()
        {
            SetDefaultUiTheme(saved);
        }
    };

    class ThemeMeasuredRow : public sky::ui::UIElement {
    public:
        ThemeMeasuredRow()
        {
            SetLayoutVolatile(false);
        }

        int measurements = 0;
        int arrangements = 0;

    protected:
        sky::ui::UISize OnMeasure(const sky::ui::UIConstraints &) override
        {
            ++measurements;
            return {40.0f, GetDefaultUiTheme().metrics.rowHeight};
        }

        void ArrangeChildren(const sky::ui::UIRect &) override
        {
            ++arrangements;
        }
    };

    class RetainedThemePanel : public sky::ui::UIElement {
    public:
        RetainedThemePanel()
        {
            SetLayoutVolatile(false);
            sky::ui::UILayoutParams fillWidth;
            fillWidth.width.mode = sky::ui::UISizeMode::FILL;
            auto outer           = std::make_unique<sky::ui::VBox>();
            auto inner           = std::make_unique<sky::ui::VBox>();
            auto row             = std::make_unique<ThemeMeasuredRow>();
            outer->SetLayout(fillWidth);
            inner->SetLayout(fillWidth);
            row->SetLayout(fillWidth);
            leaf   = static_cast<ThemeMeasuredRow *>(inner->AddChild(std::move(row)));
            nested = static_cast<sky::ui::VBox *>(outer->AddChild(std::move(inner)));
            branch = static_cast<sky::ui::VBox *>(AddChild(std::move(outer)));
        }

        ThemeMeasuredRow *leaf         = nullptr;
        sky::ui::VBox    *nested       = nullptr;
        sky::ui::VBox    *branch       = nullptr;
        int               arrangements = 0;

    protected:
        void ArrangeChildren(const sky::ui::UIRect &content) override
        {
            ++arrangements;
            branch->Layout(content);
        }
    };

    void RegisterThemePanels(EditorShell &shell, PanelRegistry &registry, RetainedThemePanel *&mainPanel, RetainedThemePanel *&floatingPanel)
    {
        registry.Register(PanelInfo{"main", "Main", 0.0f, 0.0f, nullptr});
        registry.Register(PanelInfo{"floating", "Floating", 0.0f, 0.0f, nullptr});
        shell.RegisterPanelView("main", [&mainPanel]() {
            auto panel = std::make_unique<RetainedThemePanel>();
            mainPanel  = panel.get();
            return panel;
        });
        shell.RegisterPanelView("floating", [&floatingPanel]() {
            auto panel    = std::make_unique<RetainedThemePanel>();
            floatingPanel = panel.get();
            return panel;
        });
    }

    void PaintThemeSurface(EditorShell &shell, uint32_t surface, float width, float height)
    {
        sky::ui::UIPaintContext paint;
        shell.PaintSurface(surface, paint, width, height);
    }

    ReflectedFormView *FormOf(WorldConfigPanel &panel)
    {
        const auto find = [](auto &self, sky::ui::UIElement &element) -> ReflectedFormView * {
            if (auto *form = dynamic_cast<ReflectedFormView *>(&element)) {
                return form;
            }
            for (const auto &child : element.GetChildren()) {
                if (auto *form = self(self, *child)) {
                    return form;
                }
            }
            return nullptr;
        };
        return find(find, panel);
    }

    PropertyField *GravityField(ReflectedFormView &view)
    {
        for (auto &section : view.Form().GetSections()) {
            for (auto &field : section.fields) {
                if (field.path == "gravity") {
                    return &field;
                }
            }
        }
        return nullptr;
    }

    float Gravity(ReflectedFormView &view)
    {
        const PropertyField *field = GravityField(view);
        if (field == nullptr) {
            ADD_FAILURE() << "The world config form has no gravity field";
            return 0.0f;
        }
        const sky::Any value = field->descriptor.GetValue();
        const float   *typed = value.GetAsConst<float>();
        if (typed == nullptr) {
            ADD_FAILURE() << "The gravity field is not a float";
            return 0.0f;
        }
        return *typed;
    }

    void SetGravity(WorldDocument &document, float gravity)
    {
        sky::Any *config = document.EnsureSubSystemConfig(kSubsystemName);
        ASSERT_NE(config, nullptr);
        auto *typed = config->GetAs<test::TestConfig>();
        ASSERT_NE(typed, nullptr);
        typed->gravity = gravity;
        document.ClearDirty();
    }

    struct PreviewColor {
        float r = 0.2f;
        float g = 0.4f;
        float b = 0.6f;
        float a = 1.0f;
    };

    struct PreviewConfig {
        PreviewColor       color;
        uint32_t           maxSize = 0;
        std::vector<float> items;
        std::string        heapStorage = "settings must use Any heap storage";
    };

    const sky::TypeNode *PreviewConfigType()
    {
        if (sky::Once::Get()->Run("EditorLayoutDependencyTest.PreviewTypes")) {
            auto *context = sky::SerializationContext::Get();
            context->Register<PreviewColor>("EditorBindingLifetimeColor")
                .Member<&PreviewColor::r>("r")
                .Member<&PreviewColor::g>("g")
                .Member<&PreviewColor::b>("b")
                .Member<&PreviewColor::a>("a");
            context->Register<PreviewConfig>("EditorBindingLifetimeConfig")
                .Member<&PreviewConfig::color>("color")
                .Property(static_cast<uint32_t>(sky::CommonPropertyKey::EDITOR_KIND), sky::Any(static_cast<int32_t>(PropertyEditorKind::Color)))
                .Member<&PreviewConfig::maxSize>("maxSize")
                .Member<&PreviewConfig::items>("items");
        }
        return sky::GetTypeNode(sky::TypeInfo<PreviewConfig>::RegisteredId());
    }

    PropertyField *ColorField(ReflectedFormView &view)
    {
        for (auto &section : view.Form().GetSections()) {
            for (auto &field : section.fields) {
                if (field.path == "color") {
                    return &field;
                }
            }
        }
        return nullptr;
    }

    void OpenColorPreview(ReflectedFormView &view)
    {
        auto *widget = view.Widgets().Find(PropertyEditorKind::Color);
        ASSERT_NE(widget, nullptr);
        ASSERT_NE(ColorField(view), nullptr);
        sky::ui::UIPointerEvent event;
        event.action = sky::ui::UIPointerAction::DOWN;
        event.x      = 101.0f;
        event.y      = 35.0f;
        ASSERT_TRUE(widget->OnDown(view, *ColorField(view), event, {100.0f, 30.0f, 400.0f, 60.0f}));
        event.x = 312.0f;
        event.y = 144.0f;
        EXPECT_EQ(view.OnPointerEvent(event), sky::ui::UIEventResult::HANDLED);
        EXPECT_TRUE(view.WantsPointerCapture());
        const auto color = ColorField(view)->descriptor.GetValue();
        ASSERT_NE(color.GetAsConst<PreviewColor>(), nullptr);
        EXPECT_NE(color.GetAsConst<PreviewColor>()->r, 0.2f);
    }

    ReflectedFormView *FindForm(sky::ui::UIElement &element)
    {
        if (auto *form = dynamic_cast<ReflectedFormView *>(&element)) {
            return form;
        }
        for (const auto &child : element.GetChildren()) {
            if (auto *form = FindForm(*child)) {
                return form;
            }
        }
        return nullptr;
    }

    class PreviewSettingsBuilder : public sky::AssetBuilder {
    public:
        const std::vector<std::string> &GetExtensions() const override
        {
            static const std::vector<std::string> extensions{".bindinglife"};
            return extensions;
        }
        std::string_view QueryType(const std::string &ext) const override
        {
            return ext == ".bindinglife" ? "BindingLifetimeAsset" : "";
        }
        const sky::TypeInfoRT *GetSettingsType() const override
        {
            return sky::TypeInfoObj<PreviewConfig>::Get()->RtInfo();
        }
        sky::Any MakeSettings(const sky::ProductBundleKey &, const sky::BuildSettingsOverride &values) const override
        {
            PreviewConfig settings;
            if (const auto it = values.find("maxSize"); it != values.end()) {
                settings.maxSize = static_cast<uint32_t>(std::stoul(it->second));
            }
            if (const auto it = values.find("itemCount"); it != values.end()) {
                settings.items.resize(static_cast<size_t>(std::stoul(it->second)));
            }
            return sky::Any(settings);
        }
        sky::BuildSettingsOverride DiffSettings(const sky::ProductBundleKey &, const sky::Any &edited) const override
        {
            sky::BuildSettingsOverride values;
            if (const auto *settings = edited.GetAsConst<PreviewConfig>()) {
                if (settings->maxSize != 0) {
                    values["maxSize"] = std::to_string(settings->maxSize);
                }
                if (!settings->items.empty()) {
                    values["itemCount"] = std::to_string(settings->items.size());
                }
            }
            return values;
        }
    };

    struct PreviewAssetEnvironment {
        sky::AssetBuilderManager *manager            = sky::AssetBuilderManager::Get();
        sky::AssetDataBase       *database           = sky::AssetDataBase::Get();
        EditorAssetCatalog       *catalog            = EditorAssetCatalog::Get();
        sky::CookConfig           previousCookConfig = manager->GetCookConfig();
        sky::FileSystemPtr        previousWorkspace  = database->GetWorkSpaceFs();
        PreviewSettingsBuilder   *builder            = nullptr;
        std::filesystem::path     root = std::filesystem::temp_directory_path() / ("sky_binding_lifetime_" + sky::Uuid::Create().ToString());

        PreviewAssetEnvironment()
        {
            PreviewConfigType();
            std::filesystem::create_directories(root);
            std::ofstream(root / "a.bindinglife") << "lifetime test";
            std::ofstream(root / "b.bindinglife") << "second asset";
            builder = new PreviewSettingsBuilder();
            manager->RegisterBuilder(builder);
            sky::CookConfig config;
            config.Parse(R"({"bundles":["common","other"],"presets":{"windows":["common","other"]}})");
            config.SetActivePlatform("windows");
            manager->SetCookConfig(std::move(config));
            database->Reset();
            database->SetWorkSpaceFs(new sky::NativeFileSystem(sky::FilePath(root.string())));
            database->RebuildCacheFromScan();
            catalog->Refresh();
        }

        ~PreviewAssetEnvironment()
        {
            manager->SetCookConfig(std::move(previousCookConfig));
            manager->UnRegisterBuilder(builder);
            database->Reset();
            database->SetWorkSpaceFs(previousWorkspace);
            database->RebuildCacheFromScan();
            catalog->Refresh();
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }
    };

    class EditorLayoutDependencyTest : public testing::Test {
    protected:
        void SetUp() override
        {
            test::RegisterTestTypes();
            sky::WorldSubSystemRegistry::Get().Clear();
            sky::WorldSubSystemRegistry::Get().Register(
                sky::Name(kSubsystemName), sky::WorldSubSystemRegistration{
                                               [](sky::World &, const sky::Any &) -> std::unique_ptr<sky::IWorldSubSystem> { return nullptr; },
                                               sky::TypeInfoObj<test::TestConfig>::Get()->RtInfo(),
                                               [] { return sky::Any(std::in_place_type<test::TestConfig>); },
                                               {},
                                           });
        }

        void TearDown() override
        {
            sky::WorldSubSystemRegistry::Get().Clear();
        }
    };

} // namespace

TEST_F(EditorLayoutDependencyTest, DocumentSwitchRebindsBeforeLayoutOrPaint)
{
    WorldDocument first("layout_dependency_first.world");
    WorldDocument second("layout_dependency_second.world");
    SetGravity(first, -1.0f);
    SetGravity(second, -2.0f);

    int                      providerCalls = 0;
    CountingWorldConfigPanel panel(
        nullptr,
        [&]() {
            ++providerCalls;
            return &first;
        },
        "Config");
    panel.SetDocument(&first);
    ReflectedFormView *form = FormOf(panel);
    ASSERT_NE(form, nullptr);
    EXPECT_FALSE(panel.IsLayoutVolatile());
    EXPECT_FALSE(form->IsLayoutVolatile());
    panel.SynchronizeLayout();
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    const int initialProviderCalls = providerCalls;

    panel.SetDocument(&second);
    EXPECT_FLOAT_EQ(Gravity(*form), -2.0f);
    EXPECT_TRUE(form->IsVisible());
    EXPECT_TRUE(panel.IsLayoutDirty());
    panel.SynchronizeLayout();
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    EXPECT_EQ(providerCalls, initialProviderCalls);
    EXPECT_FLOAT_EQ(Gravity(*form), -2.0f);

    panel.SetDocument(nullptr);
    EXPECT_FALSE(form->IsVisible());
    EXPECT_FALSE(form->Form().IsValid());
}

TEST_F(EditorLayoutDependencyTest, EditingAfterSwitchMarksOnlyBoundDocumentDirty)
{
    WorldDocument first("layout_dependency_edit_first.world");
    WorldDocument second("layout_dependency_edit_second.world");
    SetGravity(first, -1.0f);
    SetGravity(second, -2.0f);

    WorldConfigPanel panel(nullptr, [&]() { return &first; }, "Config");
    panel.SetDocument(&second);
    ReflectedFormView *form = FormOf(panel);
    ASSERT_NE(form, nullptr);
    PropertyField *field = GravityField(*form);
    ASSERT_NE(field, nullptr);
    first.ClearDirty();
    second.ClearDirty();

    CommandService commands;
    ASSERT_TRUE(form->Form().Edit(*field, sky::Any(-20.0f), commands));
    EXPECT_FALSE(first.IsDirty());
    EXPECT_TRUE(second.IsDirty());
    EXPECT_FLOAT_EQ(first.EnsureSubSystemConfig(kSubsystemName)->GetAsConst<test::TestConfig>()->gravity, -1.0f);
    EXPECT_FLOAT_EQ(second.EnsureSubSystemConfig(kSubsystemName)->GetAsConst<test::TestConfig>()->gravity, -20.0f);
}

TEST_F(EditorLayoutDependencyTest, UnchangedDocumentAndRectangleRetainLayout)
{
    WorldDocument document("layout_dependency_unchanged.world");
    SetGravity(document, -3.0f);
    CountingWorldConfigPanel panel(nullptr, [&]() { return &document; }, "Config");
    panel.SetDocument(&document);
    panel.SynchronizeLayout();
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    EXPECT_FALSE(panel.IsLayoutDirty());
    const int arrangements = panel.arrangements;

    document.MarkDirty(); // The unsaved marker does not change config geometry.
    panel.SetDocument(&document);
    panel.SynchronizeLayout();
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    EXPECT_EQ(panel.arrangements, arrangements);
    EXPECT_FLOAT_EQ(Gravity(*FormOf(panel)), -3.0f);
}

TEST_F(EditorLayoutDependencyTest, ExplicitRefreshRebindsReplacedConfigInSameDocument)
{
    WorldDocument document("layout_dependency_refresh.world");
    SetGravity(document, -3.0f);
    WorldConfigPanel panel(nullptr, [&]() { return &document; }, "Config");
    panel.SetDocument(&document);
    ReflectedFormView *form = FormOf(panel);
    ASSERT_NE(form, nullptr);
    EXPECT_FLOAT_EQ(Gravity(*form), -3.0f);

    document.GetWorld()->GetMutableWorldDesc()->subSystems.clear();
    panel.RefreshDocument();
    EXPECT_FLOAT_EQ(Gravity(*form), -9.81f);
    EXPECT_TRUE(panel.IsLayoutDirty());
}

TEST_F(EditorLayoutDependencyTest, ConfigReloadAbandonsPreviewAfterSameDocumentStorageReplacement)
{
    PreviewConfigType();
    sky::WorldSubSystemRegistry::Get().Clear();
    sky::WorldSubSystemRegistry::Get().Register(sky::Name(kSubsystemName),
                                                sky::WorldSubSystemRegistration{
                                                    [](sky::World &, const sky::Any &) -> std::unique_ptr<sky::IWorldSubSystem> { return nullptr; },
                                                    sky::TypeInfoObj<PreviewConfig>::Get()->RtInfo(),
                                                    [] { return sky::Any(std::in_place_type<PreviewConfig>); },
                                                    {},
                                                });
    WorldDocument    document("layout_dependency_preview_reload.world");
    WorldConfigPanel panel(nullptr, [&]() { return &document; }, "Config");
    panel.SetDocument(&document);
    panel.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
    ReflectedFormView *form = FormOf(panel);
    ASSERT_NE(form, nullptr);
    OpenColorPreview(*form);
    const auto oldLifetime = form->BindingLifetime();

    document.GetWorld()->GetMutableWorldDesc()->subSystems.clear();
    panel.RefreshDocument(); // no cancellation may dereference the released config
    EXPECT_TRUE(oldLifetime.expired());
    EXPECT_FALSE(form->WantsPointerCapture());
    auto *config = document.EnsureSubSystemConfig(kSubsystemName)->GetAs<PreviewConfig>();
    ASSERT_NE(config, nullptr);
    EXPECT_FLOAT_EQ(config->color.r, 0.2f);
    form->SynchronizeLayout();
    EXPECT_TRUE(form->Form().IsValid());
}

TEST(EditorBindingLifetimeTest, AssetViewerCancelsBeforeReloadCloseAndOwnedModelDestruction)
{
    PreviewAssetEnvironment environment;
    EditorAssetItem         item;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.bindinglife", item));
    auto viewer = std::make_unique<AssetViewerWidget>(nullptr);
    viewer->SetCatalog(environment.catalog);
    viewer->Open(item.uuid);
    viewer->Arrange({0.0f, 0.0f, 1000.0f, 700.0f});
    ReflectedFormView *form = FindForm(*viewer);
    ASSERT_NE(form, nullptr);
    OpenColorPreview(*form);
    const auto oldLifetime = form->BindingLifetime();
    form->Form().NotifyChanged(); // the real host callback persists then Reloads settings
    EXPECT_TRUE(oldLifetime.expired());
    EXPECT_FALSE(form->WantsPointerCapture());
    ASSERT_NE(ColorField(*form), nullptr);
    auto color = ColorField(*form)->descriptor.GetValue();
    ASSERT_NE(color.GetAsConst<PreviewColor>(), nullptr);
    EXPECT_FLOAT_EQ(color.GetAsConst<PreviewColor>()->r, 0.2f);

    OpenColorPreview(*form);
    viewer->Close();
    EXPECT_FALSE(form->WantsPointerCapture());
    color = ColorField(*form)->descriptor.GetValue();
    EXPECT_FLOAT_EQ(color.GetAsConst<PreviewColor>()->r, 0.2f);

    viewer->Open(item.uuid);
    viewer->Arrange({0.0f, 0.0f, 1000.0f, 700.0f});
    form = FindForm(*viewer);
    ASSERT_NE(form, nullptr);
    OpenColorPreview(*form);
    const auto closingLifetime = form->BindingLifetime();
    viewer.reset(); // models are members; child forms must clean up first
    EXPECT_TRUE(closingLifetime.expired());
}

namespace {
    PropertyField *NamedField(ReflectedFormView &view, const char *name)
    {
        for (auto &section : view.Form().GetSections()) {
            for (auto &field : section.fields) {
                if (field.path == name) {
                    return &field;
                }
            }
        }
        return nullptr;
    }

    void CollectForms(sky::ui::UIElement &element, std::vector<ReflectedFormView *> &forms)
    {
        if (auto *form = dynamic_cast<ReflectedFormView *>(&element)) {
            forms.push_back(form);
        }
        for (const auto &child : element.GetChildren()) {
            CollectForms(*child, forms);
        }
    }

    uint32_t AssetMaxSize(PreviewAssetEnvironment &environment, const sky::Uuid &asset, const std::string &target)
    {
        const auto  settings = environment.catalog->GetCookSettings(asset, target);
        const auto *typed    = settings.object.GetAsConst<PreviewConfig>();
        if (typed == nullptr) {
            ADD_FAILURE() << "Missing asset settings";
            return 0;
        }
        return typed->maxSize;
    }
} // namespace

TEST(EditorAssetSettingsHistoryTest, ViewerSessionPersistsUndoAfterHeapReloadAndIsolatesAssetsAndTargets)
{
    PreviewAssetEnvironment environment;
    EditorAssetItem         first;
    EditorAssetItem         second;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.bindinglife", first));
    ASSERT_TRUE(environment.catalog->FindByPath("Project/b.bindinglife", second));
    const auto        globalUndo = EditorCore::GetCommandService().GetUndoCount();
    AssetViewerWidget viewer(nullptr);
    viewer.SetCatalog(environment.catalog);
    viewer.Open(first.uuid);
    viewer.Arrange({0.0f, 0.0f, 1000.0f, 700.0f});
    auto *form = FindForm(viewer);
    ASSERT_NE(form, nullptr);
    ASSERT_EQ(&form->Commands(), &viewer.History());
    ASSERT_NE(NamedField(*form, "maxSize"), nullptr);
    ASSERT_TRUE(form->Form().Edit(*NamedField(*form, "maxSize"), sky::Any(uint32_t(512)), form->Commands()));
    EXPECT_EQ(viewer.History().GetUndoCount(), 1u);
    EXPECT_EQ(EditorCore::GetCommandService().GetUndoCount(), globalUndo);
    EXPECT_EQ(AssetMaxSize(environment, first.uuid, "common"), 512u);

    viewer.SynchronizeLayout(); // deferred materialization replaces the edited heap model
    // History must now address persisted identity, not that disposable model.
    auto other                                   = environment.catalog->GetCookSettings(first.uuid, "other");
    other.object.GetAs<PreviewConfig>()->maxSize = 77;
    ASSERT_TRUE(environment.catalog->ApplyCookSettings(first.uuid, "other", other.object));
    viewer.SynchronizeLayout();
    ASSERT_TRUE(viewer.Undo());
    EXPECT_EQ(AssetMaxSize(environment, first.uuid, "common"), 0u);
    EXPECT_EQ(AssetMaxSize(environment, first.uuid, "other"), 77u);
    const auto &config = environment.manager->GetCookConfig();
    EXPECT_TRUE(config.GetTargetSettings(environment.database->GetCookJson(first.uuid), "common").empty());
    const auto afterUndo = environment.database->GetCookJson(first.uuid);
    viewer.SaveAsset(); // enqueue cook from already persisted Undo state
    EXPECT_EQ(environment.database->GetCookJson(first.uuid), afterUndo);
    ASSERT_TRUE(viewer.Redo());
    EXPECT_EQ(AssetMaxSize(environment, first.uuid, "common"), 512u);
    EXPECT_EQ(AssetMaxSize(environment, first.uuid, "other"), 77u);

    form = FindForm(viewer);
    ASSERT_NE(NamedField(*form, "items"), nullptr);
    ASSERT_TRUE(form->Form().AddSequenceElement(*NamedField(*form, "items"), form->Commands()));
    auto settings = environment.catalog->GetCookSettings(first.uuid, "common");
    EXPECT_EQ(settings.object.GetAsConst<PreviewConfig>()->items.size(), 1u);
    ASSERT_TRUE(viewer.Undo());
    settings = environment.catalog->GetCookSettings(first.uuid, "common");
    EXPECT_TRUE(settings.object.GetAsConst<PreviewConfig>()->items.empty());

    viewer.Close();
    EXPECT_EQ(viewer.History().GetUndoCount(), 0u);
    EXPECT_EQ(viewer.History().GetRedoCount(), 0u);
    viewer.Open(first.uuid);
    EXPECT_FALSE(viewer.CanUndo());
    viewer.Open(second.uuid);
    form = FindForm(viewer);
    ASSERT_NE(form, nullptr);
    ASSERT_TRUE(form->Form().Edit(*NamedField(*form, "maxSize"), sky::Any(uint32_t(1024)), form->Commands()));
    EXPECT_TRUE(viewer.CanUndo());
    viewer.Open(first.uuid);
    EXPECT_FALSE(viewer.CanUndo());
    EXPECT_EQ(AssetMaxSize(environment, first.uuid, "common"), 512u);
    EXPECT_EQ(AssetMaxSize(environment, second.uuid, "common"), 1024u);

    form = FindForm(viewer);
    ASSERT_TRUE(form->Form().Edit(*NamedField(*form, "maxSize"), sky::Any(uint32_t(256)), form->Commands()));
    environment.database->RemoveAsset(first.uuid);
    environment.catalog->Refresh();
    viewer.SynchronizeLayout();
    EXPECT_FALSE(viewer.CanUndo());
    EXPECT_EQ(viewer.History().GetUndoCount(), 0u);
    EXPECT_FALSE(viewer.Undo());
}

TEST(EditorAssetSettingsHistoryTest, ShellScopedActionsUseViewerHistory)
{
    PreviewAssetEnvironment environment;
    EditorAssetItem         asset;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.bindinglife", asset));
    LayoutModel layout;
    layout.SetDefault({"main"});
    PanelRegistry registry;
    registry.Register(PanelInfo{"main", "Main", 0.0f, 0.0f, nullptr});
    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.SetAssetCatalog(environment.catalog);
    shell.Rebuild();
    shell.Layout(1000.0f, 700.0f);
    shell.OpenAssetViewer(asset.uuid);
    auto *viewer = dynamic_cast<AssetViewerWidget *>(shell.HitTest(1.0f, 1.0f));
    ASSERT_NE(viewer, nullptr);
    auto *form = FindForm(*viewer);
    ASSERT_NE(form, nullptr);
    const auto globalUndo = EditorCore::GetCommandService().GetUndoCount();
    ASSERT_TRUE(form->Form().Edit(*NamedField(*form, "maxSize"), sky::Any(uint32_t(512)), form->Commands()));
    EXPECT_EQ(EditorCore::GetCommandService().GetUndoCount(), globalUndo);
    EXPECT_TRUE(shell.InvokeAction("assetViewer.undo"));
    EXPECT_EQ(AssetMaxSize(environment, asset.uuid, "common"), 0u);
    EXPECT_TRUE(shell.InvokeAction("assetViewer.redo"));
    EXPECT_EQ(AssetMaxSize(environment, asset.uuid, "common"), 512u);
    EXPECT_EQ(EditorCore::GetCommandService().GetUndoCount(), globalUndo);
}

namespace {
    void CommitMaxSizeText(ReflectedFormView &form, const std::string &text)
    {
        auto *field = NamedField(form, "maxSize");
        ASSERT_NE(field, nullptr);
        auto *widget = form.Widgets().Find(field->kind);
        ASSERT_NE(widget, nullptr);
        sky::ui::UIPointerEvent pointer;
        pointer.action = sky::ui::UIPointerAction::DOWN;
        pointer.x      = 101.0f;
        pointer.y      = 35.0f;
        ASSERT_TRUE(widget->OnDown(form, *field, pointer, {100.0f, 30.0f, 400.0f, 60.0f}));
        sky::ui::UIKeyEvent key;
        key.action  = sky::ui::UIKeyAction::DOWN;
        key.keyCode = 0x23; // End
        form.OnKeyEvent(key);
        key.keyCode = 0x08;
        for (int digit = 0; digit < 10; ++digit) { // clear the uint32 display text
            form.OnKeyEvent(key);
        }
        sky::ui::UITextInputEvent input;
        input.text = text;
        form.OnTextInput(input);
        key.keyCode = 0x0D;
        EXPECT_EQ(form.OnKeyEvent(key), sky::ui::UIEventResult::HANDLED);
    }
} // namespace

TEST(EditorAssetSettingsHistoryTest, ObserverAssetSwitchWaitsUntilWidgetCommitEventReturns)
{
    PreviewAssetEnvironment environment;
    EditorAssetItem         first;
    EditorAssetItem         second;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.bindinglife", first));
    ASSERT_TRUE(environment.catalog->FindByPath("Project/b.bindinglife", second));
    AssetViewerWidget viewer(nullptr);
    viewer.SetCatalog(environment.catalog);
    viewer.Open(first.uuid);
    viewer.Arrange({0.0f, 0.0f, 1000.0f, 700.0f});
    auto *oldForm = FindForm(viewer);
    ASSERT_NE(oldForm, nullptr);
    bool       requested = false;
    const auto observer  = environment.catalog->AddObserver([&]() {
        if (!requested) {
            requested = true;
            EXPECT_TRUE(oldForm->Form().IsEditing());
            viewer.Open(second.uuid);
            viewer.SynchronizeLayout(); // nested layout may not destroy the committing form
            EXPECT_EQ(FindForm(viewer), oldForm);
        }
    });
    CommitMaxSizeText(*oldForm, "512");
    environment.catalog->RemoveObserver(observer);
    EXPECT_TRUE(requested);
    EXPECT_EQ(AssetMaxSize(environment, first.uuid, "common"), 512u);
    viewer.SynchronizeLayout(); // apply queued lifecycle work only after the event returned
    EXPECT_EQ(viewer.History().GetUndoCount(), 0u);
    EXPECT_EQ(viewer.History().GetRedoCount(), 0u);
    auto *nextForm = FindForm(viewer);
    ASSERT_NE(nextForm, nullptr);
    EXPECT_EQ(*NamedField(*nextForm, "maxSize")->descriptor.GetValue().GetAsConst<uint32_t>(), 0u);
    ASSERT_TRUE(nextForm->Form().Edit(*NamedField(*nextForm, "maxSize"), sky::Any(uint32_t(256)), nextForm->Commands()));
    EXPECT_EQ(AssetMaxSize(environment, second.uuid, "common"), 256u);
    EXPECT_EQ(AssetMaxSize(environment, first.uuid, "common"), 512u);
}

TEST(EditorAssetSettingsHistoryTest, ObserverTargetReplacementAndCloseWaitUntilWidgetEventReturns)
{
    PreviewAssetEnvironment environment;
    EditorAssetItem         asset;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.bindinglife", asset));
    AssetViewerWidget viewer(nullptr);
    viewer.SetCatalog(environment.catalog);
    viewer.Open(asset.uuid);
    viewer.Arrange({0.0f, 0.0f, 1000.0f, 700.0f});
    auto *oldForm = FindForm(viewer);
    ASSERT_NE(oldForm, nullptr);
    bool requested = false;
    auto observer  = environment.catalog->AddObserver([&]() {
        if (!requested) {
            requested = true;
            sky::CookConfig oneTarget;
            EXPECT_TRUE(oneTarget.Parse(R"({"bundles":["common"],"presets":{"windows":["common"]}})"));
            oneTarget.SetActivePlatform("windows");
            environment.manager->SetCookConfig(std::move(oneTarget));
            viewer.SynchronizeLayout();
            EXPECT_EQ(FindForm(viewer), oldForm);
        }
    });
    CommitMaxSizeText(*oldForm, "512");
    environment.catalog->RemoveObserver(observer);
    EXPECT_TRUE(requested);
    viewer.SynchronizeLayout();
    std::vector<ReflectedFormView *> forms;
    CollectForms(viewer, forms);
    ASSERT_EQ(forms.size(), 1u);
    EXPECT_EQ(viewer.History().GetUndoCount(), 0u);
    EXPECT_EQ(AssetMaxSize(environment, asset.uuid, "common"), 512u);

    auto *form = forms.front();
    requested  = false;
    observer   = environment.catalog->AddObserver([&]() {
        if (!requested) {
            requested = true;
            viewer.Close();
            viewer.SynchronizeLayout();
            EXPECT_TRUE(viewer.IsOpen());
        }
    });
    CommitMaxSizeText(*form, "256");
    environment.catalog->RemoveObserver(observer);
    viewer.SynchronizeLayout();
    EXPECT_TRUE(requested);
    EXPECT_FALSE(viewer.IsOpen());
    EXPECT_EQ(viewer.History().GetUndoCount(), 0u);
    EXPECT_EQ(AssetMaxSize(environment, asset.uuid, "common"), 256u);
}

TEST(EditorAssetSettingsHistoryTest, ObserverAssetSwitchDuringUndoCannotRepopulateNewSessionHistory)
{
    PreviewAssetEnvironment environment;
    EditorAssetItem         first;
    EditorAssetItem         second;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.bindinglife", first));
    ASSERT_TRUE(environment.catalog->FindByPath("Project/b.bindinglife", second));
    AssetViewerWidget viewer(nullptr);
    viewer.SetCatalog(environment.catalog);
    viewer.Open(first.uuid);
    auto *form = FindForm(viewer);
    ASSERT_NE(form, nullptr);
    ASSERT_TRUE(form->Form().Edit(*NamedField(*form, "maxSize"), sky::Any(uint32_t(512)), form->Commands()));
    viewer.SynchronizeLayout();
    bool       switched = false;
    const auto observer = environment.catalog->AddObserver([&]() {
        if (!switched) {
            switched = true;
            viewer.Open(second.uuid);
        }
    });
    EXPECT_TRUE(viewer.Undo());
    environment.catalog->RemoveObserver(observer);
    EXPECT_TRUE(switched);
    EXPECT_EQ(viewer.History().GetUndoCount(), 0u);
    EXPECT_EQ(viewer.History().GetRedoCount(), 0u);
    EXPECT_EQ(AssetMaxSize(environment, first.uuid, "common"), 0u);
    EXPECT_EQ(AssetMaxSize(environment, second.uuid, "common"), 0u);
}

TEST(EditorAssetSettingsHistoryTest, DomainHistoryCancelsColorPreviewBeforePersistenceAndDropsLateEvents)
{
    PreviewAssetEnvironment environment;
    EditorAssetItem         asset;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.bindinglife", asset));
    AssetViewerWidget viewer(nullptr);
    viewer.SetCatalog(environment.catalog);
    viewer.Open(asset.uuid);
    viewer.Arrange({0.0f, 0.0f, 1000.0f, 700.0f});
    auto *form = FindForm(viewer);
    ASSERT_NE(form, nullptr);
    ASSERT_TRUE(form->Form().Edit(*NamedField(*form, "maxSize"), sky::Any(uint32_t(512)), form->Commands()));
    viewer.SynchronizeLayout(); // replace the committed heap-backed model
    EXPECT_EQ(viewer.History().GetUndoCount(), 1u);
    EXPECT_EQ(AssetMaxSize(environment, asset.uuid, "common"), 512u);

    auto expectRestoredColor = [&]() {
        const auto  value = ColorField(*form)->descriptor.GetValue();
        const auto *color = value.GetAsConst<PreviewColor>();
        ASSERT_NE(color, nullptr);
        EXPECT_FLOAT_EQ(color->r, 0.2f);
        EXPECT_FLOAT_EQ(color->g, 0.4f);
        EXPECT_FLOAT_EQ(color->b, 0.6f);
        EXPECT_FLOAT_EQ(color->a, 1.0f);
    };
    int        persistedCallbacks = 0;
    const auto observer           = environment.catalog->AddObserver([&]() {
        ++persistedCallbacks;
        // This runs inside domain Do/Undo, before Viewer::Reload. Checking only
        // final pixels would miss a preview restored too late by that reload.
        expectRestoredColor();
    });
    struct ObserverGuard {
        EditorAssetCatalog *catalog;
        uint32_t            id;
        ~ObserverGuard()
        {
            catalog->RemoveObserver(id);
        }
    } observerGuard{environment.catalog, observer};
    OpenColorPreview(*form);
    EXPECT_EQ(viewer.History().GetUndoCount(), 1u);
    ASSERT_TRUE(viewer.Undo());
    EXPECT_EQ(persistedCallbacks, 1);
    expectRestoredColor();
    EXPECT_EQ(*NamedField(*form, "maxSize")->descriptor.GetValue().GetAsConst<uint32_t>(), 0u);
    EXPECT_EQ(AssetMaxSize(environment, asset.uuid, "common"), 0u);
    EXPECT_EQ(viewer.History().GetUndoCount(), 0u);
    EXPECT_EQ(viewer.History().GetRedoCount(), 1u);
    const auto afterUndo = environment.database->GetCookJson(asset.uuid);
    EXPECT_TRUE(environment.manager->GetCookConfig().GetTargetSettings(afterUndo, "common").empty());

    auto deliverLateEvents = [&]() {
        sky::ui::UIKeyEvent escape;
        escape.action  = sky::ui::UIKeyAction::DOWN;
        escape.keyCode = 0x1B;
        form->OnKeyEvent(escape);
        sky::ui::UIPointerEvent release;
        release.action = sky::ui::UIPointerAction::UP;
        release.x      = 312.0f;
        release.y      = 144.0f;
        form->OnPointerEvent(release);
    };
    deliverLateEvents();
    viewer.SaveAsset(); // save/cook observes the persisted Undo snapshot
    EXPECT_EQ(environment.database->GetCookJson(asset.uuid), afterUndo);
    EXPECT_EQ(viewer.History().GetUndoCount(), 0u);
    EXPECT_EQ(viewer.History().GetRedoCount(), 1u);

    OpenColorPreview(*form);
    ASSERT_TRUE(viewer.Redo());
    EXPECT_EQ(persistedCallbacks, 2);
    expectRestoredColor();
    EXPECT_EQ(*NamedField(*form, "maxSize")->descriptor.GetValue().GetAsConst<uint32_t>(), 512u);
    EXPECT_EQ(AssetMaxSize(environment, asset.uuid, "common"), 512u);
    EXPECT_EQ(viewer.History().GetUndoCount(), 1u);
    EXPECT_EQ(viewer.History().GetRedoCount(), 0u);
    const auto afterRedo = environment.database->GetCookJson(asset.uuid);
    EXPECT_EQ(environment.manager->GetCookConfig().GetTargetSettings(afterRedo, "common").at("maxSize"), "512");
    deliverLateEvents();
    EXPECT_EQ(environment.database->GetCookJson(asset.uuid), afterRedo);
    EXPECT_EQ(viewer.History().GetUndoCount(), 1u);
    EXPECT_EQ(viewer.History().GetRedoCount(), 0u);
}

TEST(EditorActionOwnerLifetimeTest, DestroyedShellActionsAreInertWhenIndependentViewerBuildsChrome)
{
    PreviewAssetEnvironment environment;
    EditorAssetItem         asset;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.bindinglife", asset));
    LayoutModel layout;
    layout.SetDefault({"main"});
    PanelRegistry panels;
    panels.Register(PanelInfo{"main", "Main", 0.0f, 0.0f, nullptr});
    auto shell = std::make_unique<EditorShell>();
    shell->SetLayout(&layout);
    shell->SetPanelRegistry(&panels);
    shell->SetAssetCatalog(environment.catalog);
    shell->Rebuild();
    shell->OpenAssetViewer(asset.uuid);
    const auto actions = EditorActionRegistry::Get()->GetActions();
    ASSERT_GE(actions.size(), 5u);
    shell.reset();

    // Do not clear the process registry: production consumers can retain these
    // actions or their snapshots after the owning window has been destroyed.
    for (const auto &action : actions) {
        if (action.id.rfind("assetViewer.", 0) == 0) {
            ASSERT_TRUE(action.enabled);
            EXPECT_FALSE(action.enabled());
            ASSERT_TRUE(action.invoke);
            action.invoke();
        }
    }
    AssetViewerWidget standalone(nullptr);
    standalone.SetCatalog(environment.catalog);
    standalone.Open(asset.uuid); // BuildToolBarItems evaluates the former owner's predicates
    standalone.Arrange({0.0f, 0.0f, 1000.0f, 700.0f});
    EXPECT_TRUE(standalone.IsOpen());
}

TEST(EditorActionOwnerLifetimeTest, OlderShellDestructionPreservesNewOwnersRegisteredActions)
{
    PreviewAssetEnvironment environment;
    EditorAssetItem         asset;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.bindinglife", asset));
    LayoutModel firstLayout;
    LayoutModel secondLayout;
    firstLayout.SetDefault({"main"});
    secondLayout.SetDefault({"main"});
    PanelRegistry panels;
    panels.Register(PanelInfo{"main", "Main", 0.0f, 0.0f, nullptr});
    auto first = std::make_unique<EditorShell>();
    first->SetLayout(&firstLayout);
    first->SetPanelRegistry(&panels);
    first->SetAssetCatalog(environment.catalog);
    first->Rebuild();
    first->OpenAssetViewer(asset.uuid);
    const auto oldActions = EditorActionRegistry::Get()->GetActions();

    EditorShell second;
    second.SetLayout(&secondLayout);
    second.SetPanelRegistry(&panels);
    second.SetAssetCatalog(environment.catalog);
    second.Rebuild();
    second.OpenAssetViewer(asset.uuid); // same ids now belong to the second shell
    second.Rebuild();                   // live-owner predicates must not read the just-destroyed viewer
    EXPECT_TRUE(second.IsAssetViewerOpen());
    first.reset();
    for (const auto &action : oldActions) {
        if (action.id.rfind("assetViewer.", 0) == 0) {
            EXPECT_FALSE(action.enabled());
            action.invoke();
        }
    }
    EXPECT_TRUE(second.IsAssetViewerOpen());
    const auto *currentClose = EditorActionRegistry::Get()->Find("assetViewer.close");
    ASSERT_NE(currentClose, nullptr);
    ASSERT_TRUE(currentClose->enabled);
    EXPECT_TRUE(currentClose->enabled());
    const auto close = currentClose->invoke;
    ASSERT_TRUE(close);
    close();
    EXPECT_FALSE(second.IsAssetViewerOpen());
}

TEST_F(EditorLayoutDependencyTest, LegacyProviderStillPollsDuringUnchangedLayout)
{
    WorldDocument first("layout_dependency_provider_first.world");
    WorldDocument second("layout_dependency_provider_second.world");
    SetGravity(first, -1.0f);
    SetGravity(second, -2.0f);
    WorldDocument           *current = &first;
    int                      calls   = 0;
    CountingWorldConfigPanel panel(
        nullptr,
        [&]() {
            ++calls;
            return current;
        },
        "Config");
    EXPECT_TRUE(panel.IsLayoutVolatile());
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    ReflectedFormView *form = FormOf(panel);
    ASSERT_NE(form, nullptr);
    const int callsBefore = calls;

    current = &second;
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    EXPECT_GT(calls, callsBefore);
    EXPECT_FLOAT_EQ(Gravity(*form), -2.0f);
    current = nullptr;
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    EXPECT_FALSE(form->IsVisible());
}

TEST_F(EditorLayoutDependencyTest, ShellNotifiesAttachedHiddenAndFloatingConfigViews)
{
    WorldDocument first("layout_dependency_shell_first.world");
    WorldDocument second("layout_dependency_shell_second.world");
    SetGravity(first, -1.0f);
    SetGravity(second, -2.0f);
    LayoutModel layout;
    layout.SetDefault({"viewport"});
    ASSERT_TRUE(layout.SplitPanel("viewport", SplitOrientation::VERTICAL, "config"));
    PanelRegistry registry;
    registry.Register(PanelInfo{"viewport", "Viewport", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"config", "Config", 0.0f, 0.0f, nullptr});
    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.SetWorldDocument(&first);
    shell.RegisterBuiltinPanelViews();
    shell.Rebuild();
    shell.Layout(1000.0f, 700.0f);
    auto *panel = dynamic_cast<WorldConfigPanel *>(shell.GetPanelView("config"));
    ASSERT_NE(panel, nullptr);
    ASSERT_TRUE(shell.IsPanelAttached("config"));
    ReflectedFormView *form = FormOf(*panel);
    ASSERT_NE(form, nullptr);
    EXPECT_FALSE(panel->IsLayoutVolatile());

    shell.SetWorldDocument(&second);
    EXPECT_FLOAT_EQ(Gravity(*form), -2.0f); // Same surface; no paint or resize.
    shell.Layout(1000.0f, 700.0f);
    EXPECT_FLOAT_EQ(Gravity(*form), -2.0f);

    shell.SetPanelVisible("config", false);
    shell.Layout(1000.0f, 700.0f);
    ASSERT_FALSE(shell.IsPanelAttached("config"));
    shell.SetWorldDocument(&first);
    EXPECT_FLOAT_EQ(Gravity(*form), -1.0f);

    shell.SetPanelVisible("config", true);
    shell.Layout(1000.0f, 700.0f);
    ASSERT_EQ(shell.FloatPanelToSurface("config", 9u, FloatingPanel{}), 9u);
    ASSERT_TRUE(shell.IsPanelFloating("config"));
    shell.SetWorldDocument(&second);
    EXPECT_FLOAT_EQ(Gravity(*form), -2.0f);
    second.GetWorld()->GetMutableWorldDesc()->subSystems.clear();
    shell.SetWorldDocument(&second); // Reloading may replace config storage without changing the document address.
    EXPECT_FLOAT_EQ(Gravity(*form), -9.81f);
    shell.SetWorldDocument(nullptr);
    EXPECT_FALSE(form->IsVisible());
    EXPECT_FALSE(form->Form().IsValid());
}

TEST(EditorLayoutThemeTest, SameScaleMetricRevisionInvalidatesNestedMeasurementCaches)
{
    ScopedDefaultTheme restore;
    SetDefaultUiTheme(MakeDarkTheme(1.0f));
    RetainedThemePanel panel;
    uint64_t           appliedRevision = 0;
    ASSERT_TRUE(SynchronizeDefaultUiTheme(panel, appliedRevision));
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    EXPECT_FLOAT_EQ(panel.branch->GetBounds().Height(), 24.0f);
    EXPECT_FLOAT_EQ(panel.nested->GetBounds().Height(), 24.0f);
    EXPECT_FLOAT_EQ(panel.leaf->GetBounds().Height(), 24.0f);
    const int measurements = panel.leaf->measurements;

    UiTheme changed           = GetDefaultUiTheme();
    changed.metrics.rowHeight = 39.0f;
    SetDefaultUiTheme(changed);
    ASSERT_TRUE(SynchronizeDefaultUiTheme(panel, appliedRevision));
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    EXPECT_FLOAT_EQ(panel.branch->GetBounds().Height(), 39.0f);
    EXPECT_FLOAT_EQ(panel.nested->GetBounds().Height(), 39.0f);
    EXPECT_FLOAT_EQ(panel.leaf->GetBounds().Height(), 39.0f);
    EXPECT_GT(panel.leaf->measurements, measurements);

    const int arrangements = panel.arrangements;
    const int measured     = panel.leaf->measurements;
    EXPECT_FALSE(SynchronizeDefaultUiTheme(panel, appliedRevision));
    panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
    EXPECT_EQ(panel.arrangements, arrangements);
    EXPECT_EQ(panel.leaf->measurements, measured);
}

TEST(EditorLayoutThemeTest, MainAndFloatingSurfacesObserveSameScaleMetricChangesAndStayIdle)
{
    ScopedDefaultTheme restore;
    LayoutModel        layout;
    layout.SetDefault({"main"});
    ASSERT_TRUE(layout.SplitPanel("main", SplitOrientation::VERTICAL, "floating"));
    PanelRegistry       registry;
    RetainedThemePanel *mainPanel     = nullptr;
    RetainedThemePanel *floatingPanel = nullptr;
    EditorShell         shell;
    shell.SetUiScale(1.0f);
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    RegisterThemePanels(shell, registry, mainPanel, floatingPanel);
    shell.Rebuild();
    shell.Layout(1000.0f, 700.0f);
    ASSERT_NE(mainPanel, nullptr);
    ASSERT_NE(floatingPanel, nullptr);
    ASSERT_EQ(shell.FloatPanelToSurface("floating", 9u, FloatingPanel{}), 9u);
    shell.SetSurfaceScale(9u, 1.0f);
    shell.Layout(1000.0f, 700.0f);
    PaintThemeSurface(shell, 9u, 500.0f, 400.0f);
    EXPECT_FLOAT_EQ(mainPanel->leaf->GetBounds().Height(), 24.0f);
    EXPECT_FLOAT_EQ(floatingPanel->leaf->GetBounds().Height(), 24.0f);

    const int mainBefore     = mainPanel->arrangements;
    const int floatingBefore = floatingPanel->arrangements;
    shell.Layout(1000.0f, 700.0f);
    PaintThemeSurface(shell, 9u, 500.0f, 400.0f);
    EXPECT_EQ(mainPanel->arrangements, mainBefore);
    EXPECT_EQ(floatingPanel->arrangements, floatingBefore);

    UiTheme changed           = GetDefaultUiTheme();
    changed.metrics.rowHeight = 41.0f;
    SetDefaultUiTheme(changed);
    shell.Layout(1000.0f, 700.0f);
    PaintThemeSurface(shell, 9u, 500.0f, 400.0f);
    EXPECT_FLOAT_EQ(mainPanel->branch->GetBounds().Height(), 41.0f);
    EXPECT_FLOAT_EQ(mainPanel->nested->GetBounds().Height(), 41.0f);
    EXPECT_FLOAT_EQ(mainPanel->leaf->GetBounds().Height(), 41.0f);
    EXPECT_FLOAT_EQ(floatingPanel->branch->GetBounds().Height(), 41.0f);
    EXPECT_FLOAT_EQ(floatingPanel->nested->GetBounds().Height(), 41.0f);
    EXPECT_FLOAT_EQ(floatingPanel->leaf->GetBounds().Height(), 41.0f);
    EXPECT_GT(mainPanel->arrangements, mainBefore);
    EXPECT_GT(floatingPanel->arrangements, floatingBefore);

    const int mainAfter     = mainPanel->arrangements;
    const int floatingAfter = floatingPanel->arrangements;
    const int mainMeasured  = mainPanel->leaf->measurements;
    const int floatMeasured = floatingPanel->leaf->measurements;
    shell.Layout(1000.0f, 700.0f);
    PaintThemeSurface(shell, 9u, 500.0f, 400.0f);
    EXPECT_EQ(mainPanel->arrangements, mainAfter);
    EXPECT_EQ(floatingPanel->arrangements, floatingAfter);
    EXPECT_EQ(mainPanel->leaf->measurements, mainMeasured);
    EXPECT_EQ(floatingPanel->leaf->measurements, floatMeasured);
}

TEST(EditorLayoutThemeTest, AlternatingSurfaceDpiReallocatesNestedRowsAndRetainsUnchangedSurface)
{
    ScopedDefaultTheme restore;
    LayoutModel        layout;
    layout.SetDefault({"main"});
    ASSERT_TRUE(layout.SplitPanel("main", SplitOrientation::VERTICAL, "floating"));
    PanelRegistry       registry;
    RetainedThemePanel *mainPanel     = nullptr;
    RetainedThemePanel *floatingPanel = nullptr;
    EditorShell         shell;
    shell.SetUiScale(1.0f);
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    RegisterThemePanels(shell, registry, mainPanel, floatingPanel);
    shell.Rebuild();
    shell.Layout(1000.0f, 700.0f);
    ASSERT_NE(mainPanel, nullptr);
    ASSERT_NE(floatingPanel, nullptr);
    ASSERT_EQ(shell.FloatPanelToSurface("floating", 9u, FloatingPanel{}), 9u);
    shell.SetSurfaceScale(9u, 2.0f);

    PaintThemeSurface(shell, 0u, 1000.0f, 700.0f);
    EXPECT_FLOAT_EQ(mainPanel->leaf->GetBounds().Height(), 24.0f);
    PaintThemeSurface(shell, 9u, 500.0f, 400.0f);
    EXPECT_FLOAT_EQ(floatingPanel->branch->GetBounds().Height(), 48.0f);
    EXPECT_FLOAT_EQ(floatingPanel->leaf->GetBounds().Height(), 48.0f);
    shell.Layout(1000.0f, 700.0f); // The normal main-window host uses Layout + Paint.
    EXPECT_FLOAT_EQ(mainPanel->branch->GetBounds().Height(), 24.0f);
    EXPECT_FLOAT_EQ(mainPanel->leaf->GetBounds().Height(), 24.0f);

    const int mainArrangements = mainPanel->arrangements;
    PaintThemeSurface(shell, 0u, 1000.0f, 700.0f);
    EXPECT_EQ(mainPanel->arrangements, mainArrangements);
    PaintThemeSurface(shell, 9u, 500.0f, 400.0f);
    EXPECT_FLOAT_EQ(floatingPanel->leaf->GetBounds().Height(), 48.0f);
    const int floatingArrangements = floatingPanel->arrangements;
    PaintThemeSurface(shell, 9u, 500.0f, 400.0f);
    EXPECT_EQ(floatingPanel->arrangements, floatingArrangements);

    shell.SetSurfaceScale(9u, 1.5f);
    PaintThemeSurface(shell, 9u, 500.0f, 400.0f);
    EXPECT_FLOAT_EQ(floatingPanel->nested->GetBounds().Height(), 36.0f);
    EXPECT_FLOAT_EQ(floatingPanel->leaf->GetBounds().Height(), 36.0f);
    PaintThemeSurface(shell, 0u, 1000.0f, 700.0f);
    EXPECT_FLOAT_EQ(mainPanel->leaf->GetBounds().Height(), 24.0f);
}

TEST(EditorLayoutThemeTest, RestoredPanelRefreshesNestedMetricsAfterDetachedThemeChange)
{
    ScopedDefaultTheme restore;
    LayoutModel        layout;
    layout.SetDefault({"main"});
    ASSERT_TRUE(layout.SplitPanel("main", SplitOrientation::VERTICAL, "floating"));
    PanelRegistry       registry;
    RetainedThemePanel *mainPanel   = nullptr;
    RetainedThemePanel *hiddenPanel = nullptr;
    EditorShell         shell;
    shell.SetUiScale(1.0f);
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    RegisterThemePanels(shell, registry, mainPanel, hiddenPanel);
    shell.Rebuild();
    shell.Layout(1000.0f, 700.0f);
    ASSERT_NE(mainPanel, nullptr);
    ASSERT_NE(hiddenPanel, nullptr);
    ASSERT_TRUE(shell.IsPanelAttached("floating"));
    const sky::ui::UIRect originalSlot = hiddenPanel->GetBounds();
    EXPECT_FLOAT_EQ(hiddenPanel->leaf->GetBounds().Height(), 24.0f);
    const int originalMeasurements = hiddenPanel->leaf->measurements;

    shell.SetPanelVisible("floating", false);
    shell.Layout(1000.0f, 700.0f);
    ASSERT_FALSE(shell.IsPanelAttached("floating"));
    UiTheme changed           = GetDefaultUiTheme();
    changed.metrics.rowHeight = 43.0f;
    SetDefaultUiTheme(changed);
    shell.Layout(1000.0f, 700.0f); // The main tree has already consumed this revision.
    EXPECT_FLOAT_EQ(mainPanel->leaf->GetBounds().Height(), 43.0f);
    EXPECT_FLOAT_EQ(hiddenPanel->leaf->GetBounds().Height(), 24.0f);
    EXPECT_EQ(hiddenPanel->leaf->measurements, originalMeasurements);

    shell.SetPanelVisible("floating", true);
    shell.Layout(1000.0f, 700.0f);
    ASSERT_TRUE(shell.IsPanelAttached("floating"));
    EXPECT_EQ(shell.GetPanelView("floating"), hiddenPanel);
    EXPECT_FLOAT_EQ(hiddenPanel->GetBounds().left, originalSlot.left);
    EXPECT_FLOAT_EQ(hiddenPanel->GetBounds().top, originalSlot.top);
    EXPECT_FLOAT_EQ(hiddenPanel->GetBounds().right, originalSlot.right);
    EXPECT_FLOAT_EQ(hiddenPanel->GetBounds().bottom, originalSlot.bottom);
    EXPECT_FLOAT_EQ(hiddenPanel->branch->GetBounds().Height(), 43.0f);
    EXPECT_FLOAT_EQ(hiddenPanel->nested->GetBounds().Height(), 43.0f);
    EXPECT_FLOAT_EQ(hiddenPanel->leaf->GetBounds().Height(), 43.0f);
    EXPECT_GT(hiddenPanel->leaf->measurements, originalMeasurements);

    const int restoredArrangements = hiddenPanel->arrangements;
    shell.Layout(1000.0f, 700.0f);
    EXPECT_EQ(hiddenPanel->arrangements, restoredArrangements);
}
