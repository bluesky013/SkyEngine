#include <core/environment/Environment.h>
#include <core/util/DynamicModule.h>
#include <editor/core/EditorCore.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ActionBarBuilder.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/UiSkin.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>

#include <filesystem>
#include <string>

using namespace sky::editor;

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
