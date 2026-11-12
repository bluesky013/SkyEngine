#include <core/environment/Environment.h>
#include <core/util/DynamicModule.h>
#include <editor/core/EditorCore.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/core/viewport/EditorCamera.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ActionBarBuilder.h>
#include <editor/widgets/AssetPreviewCameraInput.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/UiSkin.h>
#include <gtest/gtest.h>
#include <ui/UIEvent.h>
#include <ui/UIPaintContext.h>

#include <filesystem>
#include <string>

using namespace sky::editor;

TEST(EditorWidgetsTest, AssetOrbitPanAndWheelKeepContentPickSeparate)
{
    using namespace sky::ui;
    auto                    camera = MakeAssetPreviewCamera();
    AssetPreviewCameraInput navigation;
    const UIRect            image{20, 40, 820, 520};
    const auto              handle = [&](UIPointerAction action, uint32_t id, uint32_t button, float x, float y, float wheel = 0) {
        return navigation.Handle(camera, {.action = action, .pointerId = id, .button = button, .x = x, .y = y, .wheelDelta = wheel}, image);
    };
    const auto  center  = camera.pivot;
    const float initial = camera.orbitDistance;
    EXPECT_FALSE(handle(UIPointerAction::DOWN, 1, 0, 80, 90));
    EXPECT_FALSE(handle(UIPointerAction::DOWN, 1, 1, 10, 20));
    ASSERT_TRUE(handle(UIPointerAction::DOWN, 1, 1, 80, 90));
    ASSERT_TRUE(navigation.Captured());
    const auto yaw = camera.yaw;
    EXPECT_FALSE(handle(UIPointerAction::MOVE, 3, 0, 102, 110));
    EXPECT_EQ(camera.yaw, yaw);
    ASSERT_TRUE(handle(UIPointerAction::MOVE, 1, 0, 102, 110));
    EXPECT_NE(camera.yaw, yaw);
    EXPECT_FALSE(handle(UIPointerAction::UP, 1, 2, 102, 110));
    ASSERT_TRUE(handle(UIPointerAction::UP, 1, 1, 102, 110));
    EXPECT_FALSE(navigation.Captured());
    EXPECT_EQ(camera.pivot, center);
    EXPECT_FLOAT_EQ(camera.orbitDistance, initial);
    ASSERT_TRUE(handle(UIPointerAction::WHEEL, 1, 0, 80, 90, 120));
    EXPECT_LT(camera.orbitDistance, initial);
    EXPECT_GT(camera.orbitDistance, initial * .5f);
    EXPECT_EQ(camera.pivot, center);
    ASSERT_TRUE(handle(UIPointerAction::WHEEL, 1, 0, 80, 90, -120));
    EXPECT_NEAR(camera.orbitDistance, initial, .0001f);
    const auto before = camera.position;
    ASSERT_TRUE(handle(UIPointerAction::DOWN, 1, 2, 80, 90));
    ASSERT_TRUE(handle(UIPointerAction::MOVE, 1, 0, 180, 100));
    EXPECT_GT((camera.pivot - center).Dot(camera.Right()), 0.f);
    EXPECT_LT((camera.pivot - center).Dot(camera.Up()), 0.f);
    EXPECT_LT(((camera.position - before) - (camera.pivot - center)).Length(), .0001f);
    navigation.Cancel();
    EXPECT_FALSE(navigation.Captured());
    EXPECT_FALSE(handle(UIPointerAction::MOVE, 1, 0, 200, 110));
    EXPECT_TRUE(AssetPreviewCameraInput::IsFrameKey({.keyCode = 'F', .action = UIKeyAction::DOWN}));
    EXPECT_FALSE(AssetPreviewCameraInput::IsFrameKey({.keyCode = 'F', .action = UIKeyAction::DOWN, .modifiers = 1}));
}

TEST(EditorWidgetsTest, AltAssetNavigationKeepsThePressedButtonAndItsPivot)
{
    using namespace sky::ui;
    auto                    camera = MakeAssetPreviewCamera();
    AssetPreviewCameraInput navigation;
    const UIRect            image{0, 0, 800, 500};
    const auto              handle = [&](UIPointerAction action, uint32_t button, float x, float y, uint32_t modifiers = 0) {
        return navigation.Handle(camera, {.action = action, .button = button, .x = x, .y = y, .modifiers = modifiers}, image);
    };
    const auto  pivot    = camera.pivot;
    const float distance = camera.orbitDistance, yaw = camera.yaw, pitch = camera.pitch;
    EXPECT_FALSE(handle(UIPointerAction::DOWN, 0, 100, 100, kModAlt | kModCtrl));
    ASSERT_TRUE(handle(UIPointerAction::DOWN, 0, 100, 100, kModAlt));
    ASSERT_TRUE(handle(UIPointerAction::MOVE, 0, 145, 145));
    EXPECT_NEAR(camera.yaw - yaw, .15707963f, 1.e-5f);
    EXPECT_NEAR(camera.pitch - pitch, -.15707963f, 1.e-5f);
    EXPECT_FALSE(handle(UIPointerAction::UP, 1, 145, 145));
    ASSERT_TRUE(handle(UIPointerAction::UP, 0, 145, 145));
    EXPECT_EQ(camera.pivot, pivot);
    EXPECT_FLOAT_EQ(camera.orbitDistance, distance);

    const auto facing = camera.Forward();
    ASSERT_TRUE(handle(UIPointerAction::DOWN, 1, 100, 100, kModAlt));
    ASSERT_TRUE(handle(UIPointerAction::MOVE, 0, 150, 100));
    const auto horizontal = camera.orbitDistance;
    EXPECT_LT(horizontal, distance);
    ASSERT_TRUE(handle(UIPointerAction::MOVE, 0, 150, 125));
    EXPECT_LT(camera.orbitDistance, horizontal);
    ASSERT_TRUE(handle(UIPointerAction::UP, 1, 100, 100));
    EXPECT_NEAR(camera.orbitDistance, distance, 1.e-5f);
    EXPECT_EQ(camera.Forward(), facing);
    EXPECT_EQ(camera.pivot, pivot);
    ASSERT_TRUE(handle(UIPointerAction::DOWN, 2, 100, 100, kModAlt));
    ASSERT_TRUE(handle(UIPointerAction::UP, 2, 125, 115));
    EXPECT_GT((camera.pivot - pivot).Dot(camera.Right()), 0.f);
    EXPECT_LT((camera.pivot - pivot).Dot(camera.Up()), 0.f);
}

namespace {

    class EmptyCatalog : public IEditorAssetCatalog {
    public:
        std::vector<EditorAssetItem> Gather(const std::string &) const override
        {
            return {};
        }
        bool GetType(const sky::Uuid &, std::string &) const override
        {
            return false;
        }
        bool GetName(const sky::Uuid &, std::string &) const override
        {
            return false;
        }
    };

    class ProbeModule {
    public:
        explicit ProbeModule(const char *name) : module(name)
        {
        }

        bool Load()
        {
            if (!module.Load()) {
                return false;
            }
            attach        = module.GetAddress<void (*)(sky::Environment *)>("EditorWidgetsProbeAttach");
            commands      = module.GetAddress<CommandService *(*)()>("EditorWidgetsProbeCommands");
            properties    = module.GetAddress<PropertyEditorRegistry *(*)()>("EditorWidgetsProbeProperties");
            changes       = module.GetAddress<PropertyChangeNotifier *(*)()>("EditorWidgetsProbeChanges");
            theme         = module.GetAddress<const UiTheme *(*)()>("EditorWidgetsProbeTheme");
            themeRevision = module.GetAddress<uint64_t (*)()>("EditorWidgetsProbeThemeRevision");
            setScale      = module.GetAddress<void (*)(float)>("EditorWidgetsProbeSetScale");
            registerType  = module.GetAddress<void (*)(const sky::Uuid *)>("EditorWidgetsProbeRegisterType");
            hasType       = module.GetAddress<bool (*)(const sky::Uuid *)>("EditorWidgetsProbeHasType");
            execute       = module.GetAddress<void (*)(int *, int)>("EditorWidgetsProbeExecute");
            undo          = module.GetAddress<void (*)()>("EditorWidgetsProbeUndo");
            catalog       = module.GetAddress<IEditorAssetCatalog *(*)()>("EditorWidgetsProbeCatalog");
            defaultForm   = module.GetAddress<bool (*)()>("EditorWidgetsProbeDefaultForm");
            return attach && commands && properties && changes && theme && themeRevision && setScale && registerType && hasType && execute && undo &&
                   catalog && defaultForm;
        }

        sky::DynamicModule module;
        void (*attach)(sky::Environment *)      = nullptr;
        CommandService *(*commands)()           = nullptr;
        PropertyEditorRegistry *(*properties)() = nullptr;
        PropertyChangeNotifier *(*changes)()    = nullptr;
        const UiTheme *(*theme)()               = nullptr;
        uint64_t (*themeRevision)()             = nullptr;
        void (*setScale)(float)                 = nullptr;
        void (*registerType)(const sky::Uuid *) = nullptr;
        bool (*hasType)(const sky::Uuid *)      = nullptr;
        void (*execute)(int *, int)             = nullptr;
        void (*undo)()                          = nullptr;
        IEditorAssetCatalog *(*catalog)()       = nullptr;
        bool (*defaultForm)()                   = nullptr;
    };

} // namespace

TEST(EditorWidgetsTest, ChromeBuildsAndInvokesWithoutShell)
{
    auto *actions = EditorActionRegistry::Get();
    actions->Add({.id          = "widgets.save",
                  .label       = "Save",
                  .group       = EditorGroup::File,
                  .menu        = EditorMenu::File,
                  .scope       = "widgetsTest",
                  .accelerator = "Ctrl+S"});
    EditorScopeStack::Get()->Push("widgetsTest");
    struct ScopeReset {
        ~ScopeReset()
        {
            EditorScopeStack::Get()->Pop("widgetsTest");
            EditorActionRegistry::Get()->Remove("widgetsTest", "widgets.save");
        }
    } reset;

    std::string invoked;
    auto        invoke  = [&invoked](const std::string &id) { invoked = id; };
    auto        toolbar = ActionBarBuilder::BuildToolBarItems(invoke);
    auto        menus   = ActionBarBuilder::BuildMenus(invoke);
    ASSERT_EQ(toolbar.size(), 1u);
    ASSERT_EQ(menus.size(), 1u);
    ASSERT_EQ(menus[0].items.size(), 1u);
    toolbar[0].action();
    EXPECT_EQ(invoked, "widgets.save");
    invoked.clear();
    menus[0].items[0].action();
    EXPECT_EQ(invoked, "widgets.save");

    sky::ui::UIKeyEvent key;
    key.keyCode   = 'S';
    key.modifiers = kModCtrl;
    EXPECT_EQ(ActionBarBuilder::MatchAccelerator(key), "widgets.save");

    ToolBar bar(nullptr);
    bar.SetItems(std::move(toolbar));
    bar.Arrange({0.0f, 0.0f, 600.0f, GetDefaultUiTheme().metrics.toolbarHeight});
    sky::ui::UIPaintContext paint;
    paint.Begin(bar.GetBounds());
    bar.Paint(paint);
    EXPECT_FALSE(paint.GetDrawData().vertices.empty());
}

TEST(EditorWidgetsTest, DefaultsAreSharedAcrossTwoDynamicModules)
{
    // Construct defaults in the host so their storage and virtual tables outlive the modules.
    auto                &commands   = EditorCore::GetCommandService();
    auto                &properties = EditorCore::GetPropertyEditors();
    auto                &changes    = EditorCore::GetPropertyChanges();
    const UiTheme        oldTheme   = GetDefaultUiTheme();
    IEditorAssetCatalog *oldCatalog = GetEditorAssetCatalog();
    commands.Clear();

    ASSERT_TRUE(std::filesystem::exists(EDITOR_WIDGETS_PROBE_A_PATH));
    ASSERT_TRUE(std::filesystem::exists(EDITOR_WIDGETS_PROBE_B_PATH));
    ProbeModule first("EditorWidgetsProbeA");
    ProbeModule second("EditorWidgetsProbeB");
    ASSERT_TRUE(first.Load()) << first.module.GetLastError();
    ASSERT_TRUE(second.Load()) << second.module.GetLastError();
    first.attach(sky::Environment::Get());
    second.attach(sky::Environment::Get());

    struct DefaultsReset {
        CommandService      &commands;
        UiTheme              theme;
        IEditorAssetCatalog *catalog;
        ~DefaultsReset()
        {
            commands.Clear();
            SetDefaultUiTheme(theme);
            SetEditorAssetCatalog(catalog);
        }
    } reset{commands, oldTheme, oldCatalog};

    EXPECT_EQ(first.commands(), &commands);
    EXPECT_EQ(second.commands(), &commands);
    EXPECT_EQ(first.properties(), &properties);
    EXPECT_EQ(second.properties(), &properties);
    EXPECT_EQ(first.changes(), &changes);
    EXPECT_EQ(second.changes(), &changes);
    EXPECT_EQ(first.theme(), &GetDefaultUiTheme());
    EXPECT_EQ(second.theme(), &GetDefaultUiTheme());
    EXPECT_TRUE(first.defaultForm());
    EXPECT_TRUE(second.defaultForm());

    const sky::Uuid type = sky::Uuid::Create();
    first.registerType(&type);
    EXPECT_TRUE(properties.HasType(type));
    EXPECT_TRUE(second.hasType(&type));

    UiSkin         existingSkin(GetDefaultUiTheme(), nullptr);
    const uint64_t previousThemeRevision = GetDefaultUiThemeRevision();
    first.setScale(1.5f);
    EXPECT_GT(GetDefaultUiThemeRevision(), previousThemeRevision);
    EXPECT_EQ(first.themeRevision(), GetDefaultUiThemeRevision());
    EXPECT_EQ(second.themeRevision(), GetDefaultUiThemeRevision());
    EXPECT_FLOAT_EQ(second.theme()->scale, 1.5f);
    EXPECT_FLOAT_EQ(existingSkin.Theme().scale, 1.5f);

    EmptyCatalog catalog;
    SetEditorAssetCatalog(&catalog);
    EXPECT_EQ(first.catalog(), &catalog);
    EXPECT_EQ(second.catalog(), &catalog);

    int        value    = 5;
    const auto revision = changes.GetRevision();
    first.execute(&value, 42);
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(second.commands()->CanUndo());
    EXPECT_EQ(second.changes()->GetRevision(), revision + 1);
    second.undo();
    EXPECT_EQ(value, 5);
    EXPECT_EQ(changes.GetRevision(), revision + 2);
}
