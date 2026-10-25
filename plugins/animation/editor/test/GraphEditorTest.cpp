#include "animation/editor/GraphEditor.h"
#include "animation/editor/CharacterPreviewView.h"
#include "animation/editor/GraphProperties.h"
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/widgets/GraphCanvas.h>
#include <editor/widgets/ReflectedFormView.h>
#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::animation;
using namespace sky::animation::editor;
using namespace sky::editor;

namespace {
    PropertyField *FindField(PropertyField &field, std::string_view path)
    {
        if (field.path == path)
            return &field;
        for (auto &child : field.children)
            if (auto *found = FindField(child, path))
                return found;
        return nullptr;
    }
    PropertyField *Field(ReflectedFormView &view, const char *name)
    {
        for (auto &section : view.Form().GetSections())
            for (auto &field : section.fields)
                if (auto *found = FindField(field, name))
                    return found;
        return nullptr;
    }
} // namespace

TEST(AnimationPreview, ReferencePoseDisablesPlaybackAndValidSettingsClearInputErrors)
{
    auto                    preview = std::make_shared<CharacterPreview>(Uuid::Create(), "AuroraSkeleton", std::make_shared<int>(0));
    CharacterPreviewActions actions(preview, [] {}, "preview.reference-test");
    ScopedEditorActions     scope("preview.reference-test");
    for (const auto *id : {"animation.preview.play", "animation.preview.pause", "animation.preview.stop"}) {
        auto *registry = EditorActionRegistry::Get();
        EXPECT_FALSE(registry->ResolveState(registry->GetSnapshot(id)).enabled);
        EXPECT_TRUE(registry->Invoke(id));
        EXPECT_TRUE(preview->Diagnostic().empty());
    }
    auto settings = preview->Settings();
    settings.rate = std::numeric_limits<float>::quiet_NaN();
    preview->Configure(settings);
    EXPECT_FALSE(preview->Diagnostic().empty());
    EXPECT_FLOAT_EQ(preview->Settings().rate, 1.f);
    settings.rate = .5f;
    preview->Configure(settings);
    EXPECT_TRUE(preview->Diagnostic().empty());
    EXPECT_FLOAT_EQ(preview->Settings().rate, .5f);
    EXPECT_TRUE(EditorActionRegistry::Get()->Invoke("animation.preview.reload"));
}

TEST(AnimationPreview, DelayedBoundsTriggerAutomaticFocusAndFitANarrowViewport)
{
    struct Host final : IAssetPreviewProvider {
        AssetPreviewStatus output;
        AssetPreviewResult CreateSession(AssetPreviewScene) override
        {
            return AssetPreviewHandle{Uuid::Create(), Uuid::Create()};
        }
        AssetPreviewStatusResult Resize(AssetPreviewHandle, uint32_t, uint32_t) override
        {
            return output;
        }
        AssetPreviewStatusResult Update(AssetPreviewHandle, EditorCamera) override
        {
            return output;
        }
        AssetPreviewStatusResult Status(AssetPreviewHandle) const override
        {
            return output;
        }
        AssetPreviewStatusResult CloseSession(AssetPreviewHandle) override
        {
            return AssetPreviewStatus{AssetPreviewState::Closed};
        }
    } host;
    struct RestoreProvider {
        IAssetPreviewProvider *previous = AssetPreviewProviderRegistry::Get()->GetProvider();
        ~RestoreProvider()
        {
            AssetPreviewProviderRegistry::Get()->SetProvider(previous);
        }
    } restore;
    AssetPreviewProviderRegistry::Get()->SetProvider(&host);
    auto preview = std::make_shared<CharacterPreview>(Uuid::Create(), "AuroraSkeleton", std::make_shared<int>(0));
    preview->Resize(180, 600);
    const auto original = preview->Camera().position;
    preview->Tick(0);
    EXPECT_FLOAT_EQ(preview->Camera().position.z, original.z);
    EXPECT_FALSE(preview->FocusCharacter());
    host.output.state = AssetPreviewState::Ready;
    host.output.bounds.push_back({{-2, 0, -.2f}, {2, 2, .2f}});
    preview->Tick(0);
    preview->Tick(0);
    EXPECT_FLOAT_EQ(preview->Camera().pivot.y, 1.f);
    for (float x : {-2.f, 2.f})
        for (float y : {0.f, 2.f})
            for (float z : {-.2f, .2f}) {
                Vector2 screen;
                ASSERT_TRUE(preview->Camera().Project({x, y, z}, {180, 600}, screen));
                EXPECT_GT(screen.x, 0);
                EXPECT_LT(screen.x, 180);
                EXPECT_GT(screen.y, 0);
                EXPECT_LT(screen.y, 600);
            }
    CharacterPreviewActions actions(preview, [] {}, "preview.focus-test");
    ScopedEditorActions     scope("preview.focus-test");
    const auto              resourceDiagnostic = preview->Diagnostic();
    preview->ReportError("Previous focus attempt had no geometry");
    EXPECT_TRUE(EditorActionRegistry::Get()->Invoke("animation.preview.focus"));
    EXPECT_EQ(preview->Diagnostic(), resourceDiagnostic);
}

TEST(AnimationGraphEditor, ViewportLayoutKeepsCanvasAndInspectorVisibleAndSwitchesScopedTabs)
{
    GraphDocumentStorage storage{[](std::string &text) -> FileOperationStatus {
                                     text = "{}";
                                     return {};
                                 },
                                 [](std::string_view) -> FileOperationStatus { return {}; }, [](const Uuid &) {}};
    auto                 model = std::make_unique<GraphDocument>(Uuid::Create(), storage);
    ASSERT_TRUE(model->Load());
    const auto output = model->AddNode(GraphNodeKind::Output, 320, 50);
    model->AddState("Idle", output);
    auto preview = std::make_shared<CharacterPreview>(Uuid::Create(), "AuroraSkeleton", std::make_shared<int>(0));
    auto editor  = CreateGraphEditor(std::move(model), preview);
    editor->SetActionScope("graph.layout-test");
    ScopedEditorActions scope("graph.layout-test");
    auto                content = editor->CreateContent(nullptr);
    ASSERT_TRUE(editor->FitsContentToViewport());
    const auto desired = content->MeasureDesired({{}, {800, 520}});
    EXPECT_LE(desired.height, 520);
    content->Arrange({0, 0, 800, desired.height});
    const auto &children = content->GetChildren();
    ASSERT_EQ(children.size(), 5u);
    const auto *canvas = static_cast<GraphCanvas *>(children[0].get());
    ASSERT_EQ(canvas->Model().nodes.size(), 1u);
    EXPECT_EQ(canvas->Model().nodes.front().label, "Output: Idle");
    EXPECT_TRUE(children[3]->IsVisible());
    EXPECT_LT(canvas->GetBounds().bottom, children[3]->GetBounds().top);
    EXPECT_LE(children[3]->GetBounds().bottom, 520);
    EXPECT_FALSE(children[1]->IsVisible());
    EXPECT_TRUE(EditorActionRegistry::Get()->Invoke("animation.graph.inspector.graph"));
    editor->Tick(0);
    EXPECT_TRUE(children[1]->IsVisible());
    EXPECT_FALSE(children[3]->IsVisible());
    EXPECT_TRUE(EditorActionRegistry::Get()->Invoke("animation.graph.inspector.preview"));
    editor->Tick(0);
    EXPECT_TRUE(children[3]->IsVisible());
    EXPECT_FALSE(children[1]->IsVisible());
    content.reset();
    editor.reset();
    EXPECT_FALSE(EditorActionRegistry::Get()->Find("animation.graph.inspector.preview"));
}

TEST(AnimationGraphEditor, ActionsReflectedSettingsAndDeferredSaveShareDocumentHistory)
{
    std::string          source = "{}";
    int                  writes = 0, cooks = 0;
    bool                 fail = true;
    GraphDocumentStorage storage{[&](std::string &text) -> FileOperationStatus {
                                     text = source;
                                     return {};
                                 },
                                 [&](std::string_view text) -> FileOperationStatus {
                                     ++writes;
                                     if (fail)
                                         return {FileOperationError::IOError, "write failed"};
                                     source = text;
                                     return {};
                                 },
                                 [&](const Uuid &) { ++cooks; }};
    auto                 model    = std::make_unique<GraphDocument>(Uuid::Create(), storage);
    auto                *document = model.get();
    ASSERT_TRUE(model->Load());
    auto editor  = CreateGraphEditor(std::move(model));
    auto content = editor->CreateContent(nullptr);
    content->Arrange({0, 0, 700, 1000});
    EditorScopeStack::Get()->Push("assetViewer");
    ASSERT_TRUE(EditorActionRegistry::Get()->Invoke("animation.graph.clip"));
    ASSERT_TRUE(EditorActionRegistry::Get()->Invoke("animation.graph.output"));
    EXPECT_EQ(document->Source().definition.nodes.size(), 2u);
    ASSERT_TRUE(editor->History().Undo());
    EXPECT_EQ(document->Source().definition.nodes.size(), 1u);
    ASSERT_TRUE(editor->History().Redo());
    editor->Tick(0);
    std::vector<ReflectedFormView *> forms;
    for (const auto &child : content->GetChildren())
        if (std::string_view(child->GetTypeName()) == "ReflectedFormView")
            forms.push_back(static_cast<ReflectedFormView *>(child.get()));
    ASSERT_EQ(forms.size(), 2u);
    auto &form       = *forms.front();
    auto *parameters = Field(form, "parameters");
    ASSERT_NE(parameters, nullptr);
    ASSERT_TRUE(form.Form().AddSequenceElement(*parameters, editor->History()));
    ASSERT_EQ(document->Source().definition.parameters.size(), 1u);
    const auto parameter = document->Source().definition.parameters.front().id;
    editor->Tick(0);
    auto properties                            = ReadGraphProperties(document->Source().definition);
    properties.parameters.front().name         = "Speed";
    properties.parameters.front().floatDefault = .25f;
    parameters                                 = Field(form, "parameters");
    ASSERT_NE(parameters, nullptr);
    ASSERT_TRUE(form.Form().Edit(*parameters, Any(properties.parameters), editor->History()));
    editor->Tick(0);
    EXPECT_EQ(document->Source().definition.parameters.front().id, parameter);
    EXPECT_EQ(document->Source().definition.parameters.front().name, "Speed");
    EXPECT_FLOAT_EQ(std::get<float>(document->Source().definition.parameters.front().value), .25f);
    ASSERT_TRUE(editor->History().Undo());
    editor->Tick(0);
    EXPECT_EQ(document->Source().definition.parameters.front().name, "Parameter");
    ASSERT_TRUE(editor->History().Redo());
    editor->RequestSave();
    editor->RequestSave();
    EXPECT_EQ(writes, 0);
    editor->Tick(0);
    EXPECT_EQ(writes, 1);
    EXPECT_EQ(cooks, 0);
    EXPECT_TRUE(editor->IsDirty());
    EXPECT_NE(editor->Status().find("Unsaved draft"), std::string::npos);
    EXPECT_NE(editor->Status().find("write failed"), std::string::npos);
    fail = false;
    editor->RequestSave();
    editor->Tick(0);
    EXPECT_EQ(cooks, 1);
    EXPECT_FALSE(editor->IsDirty());
    GraphSource reopened;
    ASSERT_TRUE(ParseGraphSource(source, reopened));
    EXPECT_EQ(reopened.definition.parameters.front().name, "Speed");
    const auto staleAction = EditorActionRegistry::Get()->GetSnapshot("animation.graph.clip");
    content.reset();
    editor.reset();
    EXPECT_FALSE(EditorActionRegistry::Get()->IsCurrent(staleAction));
    EXPECT_FALSE(EditorActionRegistry::Get()->Invoke(staleAction));
    EditorScopeStack::Get()->Pop("assetViewer");
}

TEST(AnimationGraphEditor, NestedStateAndTransitionFieldsSurviveRefreshUndoAndSourceReopen)
{
    std::string          source = "{}";
    GraphDocumentStorage storage{[&](std::string &text) -> FileOperationStatus {
                                     text = source;
                                     return {};
                                 },
                                 [&](std::string_view text) -> FileOperationStatus {
                                     source = text;
                                     return {};
                                 },
                                 [](const Uuid &) {}};
    auto                 model    = std::make_unique<GraphDocument>(Uuid::Create(), storage);
    auto                *document = model.get();
    ASSERT_TRUE(model->Load());
    auto editor = CreateGraphEditor(std::move(model));
    editor->SetActionScope("graph.nested-fields");
    auto               content = editor->CreateContent(nullptr);
    ReflectedFormView *form    = nullptr;
    for (const auto &child : content->GetChildren())
        if (std::string_view(child->GetTypeName()) == "ReflectedFormView") {
            form = static_cast<ReflectedFormView *>(child.get());
            break;
        }
    ASSERT_NE(form, nullptr);
    const auto add = [&](const char *path) {
        auto *field = Field(*form, path);
        if (!field)
            return false;
        const bool result = form->Form().AddSequenceElement(*field, editor->History());
        editor->Tick(0);
        return result;
    };
    const auto edit = [&](const char *path, Any value) {
        auto *field = Field(*form, path);
        if (!field)
            return false;
        const bool result = form->Form().Edit(*field, std::move(value), editor->History());
        editor->Tick(0);
        return result;
    };
    ASSERT_TRUE(add("parameters"));
    ASSERT_TRUE(edit("parameters[0].name", Any(std::string("Speed"))));
    ASSERT_TRUE(add("states"));
    ASSERT_TRUE(edit("states[0].name", Any(std::string("Idle"))));
    ASSERT_TRUE(add("states"));
    ASSERT_TRUE(edit("states[1].name", Any(std::string("Walk"))));
    const auto idle  = document->Source().definition.states[0].id;
    const auto walk  = document->Source().definition.states[1].id;
    const auto speed = document->Source().definition.parameters[0].id;
    ASSERT_TRUE(edit("entryState", Any(idle)));
    ASSERT_TRUE(add("transitions"));
    ASSERT_TRUE(edit("transitions[0].from", Any(idle)));
    ASSERT_TRUE(edit("transitions[0].to", Any(walk)));
    ASSERT_TRUE(edit("transitions[0].duration", Any(.3f)));
    ASSERT_TRUE(add("transitions[0].conditions"));
    ASSERT_TRUE(edit("transitions[0].conditions[0].parameter", Any(speed)));
    ASSERT_TRUE(edit("transitions[0].conditions[0].type", Any(GraphParameterType::Float)));
    ASSERT_TRUE(edit("transitions[0].conditions[0].comparison", Any(GraphComparison::Greater)));
    ASSERT_TRUE(edit("transitions[0].conditions[0].floatValue", Any(.5f)));
    const auto transition = document->Source().definition.transitions[0].id;
    EXPECT_FALSE(edit("states[0].id", Any(Uuid::Create())));
    ASSERT_TRUE(editor->History().Undo());
    editor->Tick(0);
    EXPECT_FLOAT_EQ(std::get<float>(document->Source().definition.transitions[0].conditions[0].value), 0.f);
    ASSERT_TRUE(editor->History().Redo());
    editor->Tick(0);
    EXPECT_FLOAT_EQ(std::get<float>(document->Source().definition.transitions[0].conditions[0].value), .5f);
    editor->RequestSave();
    editor->Tick(0);
    EXPECT_FALSE(editor->IsDirty());
    GraphSource reopened;
    ASSERT_TRUE(ParseGraphSource(source, reopened));
    EXPECT_EQ(reopened.definition.entryState, idle);
    EXPECT_EQ(reopened.definition.states[1].id, walk);
    EXPECT_EQ(reopened.definition.transitions[0].id, transition);
    EXPECT_EQ(reopened.definition.transitions[0].conditions[0].parameter, speed);
    EXPECT_FLOAT_EQ(std::get<float>(reopened.definition.transitions[0].conditions[0].value), .5f);
    content.reset();
    editor.reset();
}
