#include "animation/editor/GraphEditor.h"

#include "animation/editor/CharacterPreviewView.h"
#include "animation/editor/GraphProperties.h"
#include <algorithm>
#include <core/type/TypeInfo.h>
#include <editor/core/EditorCore.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/widgets/AssetReflectedWidget.h>
#include <editor/widgets/GraphCanvas.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/ToolBar.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::animation::editor {
    using namespace sky::editor;
    using namespace sky::ui;
    namespace {
        class GraphReferences final : public IEditorAssetCatalog {
        public:
            explicit GraphReferences(const GraphDocument &document) : document(document)
            {
            }
            std::vector<EditorAssetItem> Gather(const std::string &type) const override
            {
                std::vector<EditorAssetItem> result;
                const auto                  &graph = document.Source().definition;
                if (type == "GraphNode") {
                    for (const auto &node : graph.nodes)
                        result.push_back({.uuid = node.id,
                                          .name = std::string(node.kind == GraphNodeKind::Clip     ? "Clip "
                                                              : node.kind == GraphNodeKind::Blend2 ? "Blend2 "
                                                                                                   : "Output ") +
                                                  node.id.ToString().substr(0, 8),
                                          .type = type});
                } else if (type == "GraphParameter") {
                    for (const auto &p : graph.parameters)
                        result.push_back({.uuid = p.id, .name = p.name, .type = type});
                } else if (type == "GraphState") {
                    for (const auto &s : graph.states)
                        result.push_back({.uuid = s.id, .name = s.name, .type = type});
                } else if (type == "GraphTransition") {
                    for (const auto &t : graph.transitions)
                        result.push_back({.uuid = t.id, .name = t.id.ToString(), .type = type});
                } else if (auto *catalog = GetEditorAssetCatalog()) {
                    return catalog->Gather(type);
                }
                return result;
            }
            bool GetType(const Uuid &id, std::string &out) const override
            {
                for (const auto *type : {"GraphNode", "GraphParameter", "GraphState", "GraphTransition"})
                    for (const auto &item : Gather(type))
                        if (item.uuid == id) {
                            out = type;
                            return true;
                        }
                auto *catalog = GetEditorAssetCatalog();
                return catalog && catalog->GetType(id, out);
            }
            bool GetName(const Uuid &id, std::string &out) const override
            {
                for (const auto *type : {"GraphNode", "GraphParameter", "GraphState", "GraphTransition"})
                    for (const auto &item : Gather(type))
                        if (item.uuid == id) {
                            out = item.name;
                            return true;
                        }
                auto *catalog = GetEditorAssetCatalog();
                return catalog && catalog->GetName(id, out);
            }

        private:
            const GraphDocument &document;
        };
        class GraphEditorView final : public UIElement {
        public:
            enum class Inspector { Graph, Node, Preview };
            GraphEditorView(GraphDocument &sourceDocument, UITextSystem *text, CharacterPreview *preview)
                : document(sourceDocument), references(sourceDocument)
            {
                canvas = static_cast<GraphCanvas *>(AddChild(std::make_unique<GraphCanvas>(text)));
                const ReflectedFormContext context{document.Commands(), EditorCore::GetPropertyEditors(), EditorCore::GetPropertyChanges(),
                                                   GetDefaultUiTheme()};
                settings =
                    static_cast<ReflectedFormView *>(AddChild(std::make_unique<ReflectedFormView>(text, "Graph and parameter defaults", context)));
                nodeSettings = static_cast<ReflectedFormView *>(AddChild(std::make_unique<ReflectedFormView>(text, "Selected node", context)));
                if (preview)
                    previewSettings = static_cast<CharacterPreviewPanel *>(AddChild(std::make_unique<CharacterPreviewPanel>(*preview, text)));
                inspector = pendingInspector = preview ? Inspector::Preview : Inspector::Graph;
                tabs                         = static_cast<ToolBar *>(AddChild(std::make_unique<ToolBar>(text)));
                tabs->SetTabStyle(true);
                settings->Widgets().Register(PropertyEditorKind::Asset, std::make_shared<AssetReflectedWidget>(&references));
                nodeSettings->Widgets().Register(PropertyEditorKind::Asset, std::make_shared<AssetReflectedWidget>(&references));
                settings->Form().SetEditCommandFactory([this](const PropertyDescriptor &field, Any value) {
                    const auto previous = properties;
                    if (!field.SetValue(value))
                        return UndoCommandPtr{};
                    auto next = document.Source();
                    WriteGraphProperties(properties, next.definition);
                    properties = previous;
                    return document.MakeEdit("Edit Graph settings", std::move(next));
                });
                nodeSettings->Form().SetEditCommandFactory([this](const PropertyDescriptor &field, Any value) {
                    const auto oldClip  = clip;
                    const auto oldBlend = blend;
                    if (!field.SetValue(value))
                        return UndoCommandPtr{};
                    auto next  = document.Source();
                    auto found = std::ranges::find(next.definition.nodes, selected, &GraphNode::id);
                    if (found != next.definition.nodes.end()) {
                        if (found->kind == GraphNodeKind::Clip) {
                            found->clip = clip.clip;
                            found->loop = clip.loop;
                            found->rate = clip.rate;
                        } else if (found->kind == GraphNodeKind::Blend2) {
                            found->alpha          = blend.alpha;
                            found->alphaParameter = blend.alphaParameter;
                        }
                    }
                    clip  = oldClip;
                    blend = oldBlend;
                    return document.MakeEdit("Edit node settings", std::move(next));
                });
                canvas->SetCallbacks({.selection =
                                          [this](std::vector<Uuid> ids) {
                                              selected         = ids.empty() ? Uuid{} : ids.back();
                                              selectionChanged = true;
                                              if (!ids.empty())
                                                  pendingInspector = Inspector::Node;
                                          },
                                      .moved =
                                          [this](std::vector<GraphCanvasPosition> positions) {
                                              std::vector<GraphNodePosition> next;
                                              for (const auto &p : positions)
                                                  next.push_back({p.node, p.position.x, p.position.y});
                                              document.MoveNodes(next);
                                          },
                                      .connected =
                                          [this](GraphCanvasLink link) {
                                              document.Connect(link.from.node, link.to.node, link.to.pin == 2 ? GraphPin::InputB : GraphPin::InputA);
                                          },
                                      .viewChanged =
                                          [this](GraphCanvasView view) {
                                              auto next        = document.Source();
                                              next.editor.panX = view.pan.x;
                                              next.editor.panY = view.pan.y;
                                              next.editor.zoom = view.zoom;
                                              document.Apply("Change Graph view", std::move(next));
                                          }});
                Sync();
            }
            ~GraphEditorView() override
            {
                Cancel();
                ClearChildren();
            }
            bool Editing() const
            {
                return canvas->WantsPointerCapture() || settings->HasActiveInteraction() || nodeSettings->HasActiveInteraction() ||
                       settings->Form().IsEditing() || nodeSettings->Form().IsEditing() || (previewSettings && previewSettings->Editing());
            }
            CommandService &History()
            {
                return previewSettings && focus == previewSettings ? previewSettings->History() : document.Commands();
            }
            bool CanvasFocused() const
            {
                return focus == canvas;
            }
            void Cancel()
            {
                canvas->CancelInteraction();
                settings->CancelInteractions();
                nodeSettings->CancelInteractions();
                if (previewSettings)
                    previewSettings->Cancel();
                captured = nullptr;
            }
            void Sync()
            {
                if (Editing())
                    return;
                inspector = pendingInspector;
                settings->SetVisible(inspector == Inspector::Graph);
                nodeSettings->SetVisible(inspector == Inspector::Node);
                if (previewSettings)
                    previewSettings->SetVisible(inspector == Inspector::Preview);
                if (focus && !focus->IsVisible())
                    focus = inspector == Inspector::Graph  ? static_cast<UIElement *>(settings)
                            : inspector == Inspector::Node ? static_cast<UIElement *>(nodeSettings)
                                                           : previewSettings;
                tabs->RefreshActionStates();
                if (previewSettings)
                    previewSettings->Sync();
                if (revision != document.Revision()) {
                    Cancel();
                    revision           = document.Revision();
                    const auto &source = document.Source();
                    properties         = ReadGraphProperties(source.definition);
                    if (settingsBound)
                        settings->Refresh();
                    else {
                        settings->Bind({&properties, GetTypeNode(TypeInfo<GraphProperties>::RegisteredId())});
                        settingsBound = true;
                    }
                    GraphCanvasModel model;
                    const Name       pose("AnimationPose");
                    for (const auto &node : source.definition.nodes) {
                        GraphCanvasNode item;
                        item.id    = node.id;
                        item.label = node.kind == GraphNodeKind::Clip ? "Clip" : node.kind == GraphNodeKind::Blend2 ? "Blend2" : "Output";
                        std::string name;
                        if (node.kind == GraphNodeKind::Clip && references.GetName(node.clip, name))
                            item.label += ": " + name;
                        else if (node.kind == GraphNodeKind::Output) {
                            const auto state = std::ranges::find(source.definition.states, node.id, &GraphState::output);
                            if (state != source.definition.states.end())
                                item.label += ": " + state->name;
                        }
                        if (const auto position = std::ranges::find(source.editor.nodes, node.id, &GraphNodePosition::node);
                            position != source.editor.nodes.end())
                            item.position = {position->x, position->y};
                        if (node.kind != GraphNodeKind::Output)
                            item.pins.push_back({0, "Pose", pose, true});
                        if (node.kind != GraphNodeKind::Clip)
                            item.pins.push_back({1, node.kind == GraphNodeKind::Output ? "Pose" : "A", pose, false});
                        if (node.kind == GraphNodeKind::Blend2)
                            item.pins.push_back({2, "B", pose, false});
                        item.invalid = std::ranges::any_of(document.Diagnostics().diagnostics, [&](const auto &d) { return d.object == node.id; });
                        model.nodes.push_back(std::move(item));
                    }
                    for (const auto &link : source.definition.links)
                        model.links.push_back({{link.from, 0}, {link.to, link.input == GraphPin::InputB ? 2u : 1u}});
                    if (!canvas->SetModel(std::move(model)) || !canvas->SetView({{source.editor.panX, source.editor.panY}, source.editor.zoom}))
                        presentationError = "Graph canvas cannot display this layout";
                    else
                        presentationError.clear();
                    selectionChanged = true;
                }
                if (selectionChanged) {
                    nodeSettings->CancelInteractions();
                    nodeSettings->Unbind();
                    const auto &nodes = document.Source().definition.nodes;
                    const auto  found = std::ranges::find(nodes, selected, &GraphNode::id);
                    if (found != nodes.end() && found->kind == GraphNodeKind::Clip) {
                        clip = {found->clip, found->loop, found->rate};
                        nodeSettings->Bind({&clip, GetTypeNode(TypeInfo<ClipNodeProperties>::RegisteredId())});
                    } else if (found != nodes.end() && found->kind == GraphNodeKind::Blend2) {
                        blend = {found->alpha, found->alphaParameter};
                        nodeSettings->Bind({&blend, GetTypeNode(TypeInfo<BlendNodeProperties>::RegisteredId())});
                    }
                    selectionChanged = false;
                }
                settings->SynchronizeLayout();
                nodeSettings->SynchronizeLayout();
                MarkLayoutDirty();
            }
            const std::vector<Uuid> &Selection() const
            {
                return canvas->Selection();
            }
            void SetInspectorItems(std::vector<ToolBar::Item> items)
            {
                tabs->SetItems(std::move(items));
            }
            void RequestInspector(Inspector value)
            {
                pendingInspector = value;
            }
            Inspector CurrentInspector() const
            {
                return inspector;
            }
            void ApplyPreviewDefaults()
            {
                if (previewSettings)
                    previewSettings->ApplyDefaults();
            }
            void FrameGraph()
            {
                if (!canvas->FrameAll())
                    presentationError = "Graph cannot be framed in the current viewport";
            }
            void UpdatePlayback(const AnimationInstanceStatus &status)
            {
                std::vector<Uuid> active;
                for (const auto &node : status.nodes)
                    if (node.active)
                        active.push_back(node.node);
                canvas->SetActive(active);
            }
            void Select(const std::vector<Uuid> &ids)
            {
                canvas->SetSelection(ids);
                selected         = ids.empty() ? Uuid{} : ids.back();
                selectionChanged = true;
                if (!ids.empty())
                    pendingInspector = Inspector::Node;
            }
            bool WantsPointerCapture() const override
            {
                return captured || settings->WantsPointerCapture() || nodeSettings->WantsPointerCapture() ||
                       (previewSettings && previewSettings->WantsPointerCapture());
            }
            UIEventResult OnPointerEvent(const UIPointerEvent &event) override
            {
                UIElement *target = captured;
                if (!target && settings->WantsPointerCapture())
                    target = settings;
                if (!target && nodeSettings->WantsPointerCapture())
                    target = nodeSettings;
                if (!target && previewSettings && previewSettings->WantsPointerCapture())
                    target = previewSettings;
                if (!target) {
                    for (auto *child : {static_cast<UIElement *>(tabs), static_cast<UIElement *>(canvas), static_cast<UIElement *>(settings),
                                        static_cast<UIElement *>(nodeSettings), static_cast<UIElement *>(previewSettings)})
                        if (child && child->IsVisible() && child->GetBounds().Contains(event.x, event.y))
                            target = child;
                }
                if (!target)
                    return UIEventResult::UNHANDLED;
                if (event.action == UIPointerAction::DOWN && target != tabs)
                    focus = target;
                const auto result = target->OnPointerEvent(event);
                captured          = target->WantsPointerCapture() ? target : nullptr;
                return result;
            }
            void OnPointerCancel(uint32_t pointer) override
            {
                if (captured)
                    captured->OnPointerCancel(pointer);
                captured = nullptr;
            }
            UIEventResult OnKeyEvent(const UIKeyEvent &event) override
            {
                return focus ? focus->OnKeyEvent(event) : UIEventResult::UNHANDLED;
            }
            UIEventResult OnTextInput(const UITextInputEvent &event) override
            {
                return focus ? focus->OnTextInput(event) : UIEventResult::UNHANDLED;
            }
            const std::string &Error() const
            {
                return presentationError;
            }

        protected:
            UISize OnMeasure(const UIConstraints &constraints) override
            {
                const auto &m = GetDefaultUiTheme().metrics;
                return constraints.Constrain({m.fieldWidth * 3, m.rowHeight * 30});
            }
            void ArrangeChildren(const UIRect &content) override
            {
                const auto &m           = GetDefaultUiTheme().metrics;
                const auto  graphBottom = content.top + std::max(0.f, content.Height() - m.toolbarHeight) * .6f;
                canvas->Arrange({content.left, content.top, content.right, graphBottom});
                tabs->Arrange({content.left, graphBottom, content.right, graphBottom + m.toolbarHeight});
                const UIRect inspectorBounds{content.left, graphBottom + m.toolbarHeight, content.right, content.bottom};
                nodeSettings->Arrange(inspectorBounds);
                settings->Arrange(inspectorBounds);
                if (previewSettings)
                    previewSettings->Arrange(inspectorBounds);
            }

        private:
            GraphDocument         &document;
            GraphReferences        references;
            GraphProperties        properties;
            ClipNodeProperties     clip;
            BlendNodeProperties    blend;
            GraphCanvas           *canvas   = nullptr;
            ReflectedFormView     *settings = nullptr, *nodeSettings = nullptr;
            CharacterPreviewPanel *previewSettings = nullptr;
            ToolBar               *tabs            = nullptr;
            Inspector              inspector = Inspector::Graph, pendingInspector = Inspector::Graph;
            UIElement             *focus = nullptr, *captured = nullptr;
            Uuid                   selected;
            uint64_t               revision         = 0;
            bool                   selectionChanged = false;
            bool                   settingsBound    = false;
            std::string            presentationError;
        };

        class GraphEditorDocument final : public IAssetViewDocument {
        public:
            GraphEditorDocument(std::unique_ptr<GraphDocument> document, std::shared_ptr<CharacterPreview> preview)
                : document(std::move(document)), preview(std::move(preview))
            {
            }
            ~GraphEditorDocument() override
            {
                lifetime.reset();
                previewActions.reset();
                if (preview)
                    preview->Close();
                for (const auto &id : actionIds)
                    EditorActionRegistry::Get()->Remove(scope, id);
            }
            void SetActionScope(std::string value) override
            {
                scope = std::move(value);
            }
            std::unique_ptr<UIElement> CreateContent(UITextSystem *text) override
            {
                auto content = std::make_unique<GraphEditorView>(*document, text, preview.get());
                view         = content.get();
                if (preview)
                    previewActions = std::make_unique<CharacterPreviewActions>(preview, [this] { view->ApplyPreviewDefaults(); }, scope);
                AddAction("clip", "Add Clip", [this] { AddNode(GraphNodeKind::Clip); });
                AddAction("frame", "Frame all Graph nodes", [this] { view->FrameGraph(); }, "Home", "focus", EditorGroup::Navigation);
                AddAction("blend", "Add Blend2", [this] { AddNode(GraphNodeKind::Blend2); });
                AddAction("output", "Add Output", [this] { AddNode(GraphNodeKind::Output); });
                AddAction("delete", "Delete Selected Nodes", [this] { document->Remove(view->Selection()); }, "Delete");
                AddAction(
                    "copy", "Duplicate Selected Nodes",
                    [this] {
                        const auto &m = GetDefaultUiTheme().metrics;
                        const auto  ids =
                            document->CopyNodes(view->Selection(), m.rowHeight / GetDefaultUiTheme().scale, m.rowHeight / GetDefaultUiTheme().scale);
                        view->Sync();
                        view->Select(ids);
                    },
                    "Ctrl+D");
                AddAction("disconnect", "Disconnect Selected Inputs", [this] {
                    auto next = document->Source();
                    std::erase_if(next.definition.links,
                                  [&](const auto &link) { return std::ranges::find(view->Selection(), link.to) != view->Selection().end(); });
                    document->Apply("Disconnect inputs", std::move(next));
                });
                AddAction("state", "Add State from Selected Output", [this] {
                    document->AddState("State " + std::to_string(document->Source().definition.states.size() + 1),
                                       view->Selection().empty() ? Uuid{} : view->Selection().back());
                });
                AddAction("parameter", "Add Float Parameter", [this] {
                    document->AddParameter("Parameter " + std::to_string(document->Source().definition.parameters.size() + 1),
                                           GraphParameterType::Float);
                });
                std::vector<ToolBar::Item> inspectorItems;
                for (auto panel : {GraphEditorView::Inspector::Graph, GraphEditorView::Inspector::Node, GraphEditorView::Inspector::Preview}) {
                    if (panel == GraphEditorView::Inspector::Preview && !preview)
                        continue;
                    const auto                suffix = panel == GraphEditorView::Inspector::Graph  ? "graph"
                                                       : panel == GraphEditorView::Inspector::Node ? "node"
                                                                                                   : "preview";
                    const auto                label  = panel == GraphEditorView::Inspector::Graph  ? "Graph settings"
                                                       : panel == GraphEditorView::Inspector::Node ? "Selected node"
                                                                                                   : "Preview settings";
                    const auto                id     = std::string("animation.graph.inspector.") + suffix;
                    const std::weak_ptr<void> alive  = lifetime;
                    EditorAction              action;
                    action.id          = id;
                    action.label       = label;
                    action.menu        = EditorMenu::View;
                    action.submenu     = "Inspector";
                    action.scope       = scope;
                    action.moduleLease = preview ? preview->CodeLease() : nullptr;
                    action.enabled     = [this, alive] { return !alive.expired() && view && !view->Editing(); };
                    action.checkable   = true;
                    action.checked     = [this, alive, panel] { return !alive.expired() && view->CurrentInspector() == panel; };
                    action.invoke      = [this, alive, panel] {
                        if (!alive.expired())
                            view->RequestInspector(panel);
                    };
                    EditorActionRegistry::Get()->Add(std::move(action));
                    actionIds.push_back(id);
                    const ScopedEditorActions active(scope);
                    const auto                snapshot = EditorActionRegistry::Get()->GetSnapshot(id);
                    ToolBar::Item             item;
                    item.id           = id;
                    item.label        = label;
                    item.checkable    = true;
                    item.action       = [snapshot] { EditorActionRegistry::Get()->Invoke(snapshot); };
                    item.resolveState = [snapshot] { return EditorActionRegistry::Get()->ResolveState(snapshot); };
                    inspectorItems.push_back(std::move(item));
                }
                view->SetInspectorItems(std::move(inspectorItems));
                return content;
            }
            bool FitsContentToViewport() const override
            {
                return true;
            }
            std::unique_ptr<UIElement> CreatePreview(UITextSystem *text) override
            {
                return preview ? CreateCharacterViewport(*preview, text) : nullptr;
            }
            CommandService &History() override
            {
                return view ? view->History() : document->Commands();
            }
            bool IsDirty() const override
            {
                return document->IsDirty();
            }
            bool IsEditing() const override
            {
                return view && view->Editing();
            }
            void CancelInteractions() override
            {
                if (view)
                    view->Cancel();
            }
            void RequestSave() override
            {
                document->RequestSave();
            }
            bool IsSavePending() const override
            {
                return document->IsSavePending();
            }
            std::string Status() const override
            {
                std::string status = document->IsDirty() ? "Unsaved draft. " : "Saved. ";
                if (!document->LastError().empty())
                    return status + document->LastError();
                if (view && !view->Error().empty())
                    return status + view->Error();
                if (!document->Diagnostics())
                    return status + document->Diagnostics().diagnostics.front().message;
                if (preview) {
                    const auto &state = preview->Playback();
                    if (!preview->Diagnostic().empty())
                        return status + preview->Diagnostic();
                    if (state.state == AnimationSourceState::Ready) {
                        const auto active = std::ranges::find(document->Source().definition.states, state.activeState, &GraphState::id);
                        status += "Preview r" + std::to_string(state.animation.revision) + (state.retainedVersion ? " (retained)" : "");
                        if (active != document->Source().definition.states.end())
                            status += " | " + active->name;
                        if (state.transition)
                            status += " | Transition " + std::to_string(int(state.transitionProgress * 100.f)) + "%";
                        status += ". ";
                    }
                }
                return status + "Executable structure is valid";
            }
            void Tick(float delta) override
            {
                if (!IsEditing()) {
                    document->FlushPendingSave();
                    if (view)
                        view->Sync();
                }
                if (preview) {
                    preview->Tick(delta);
                    if (view)
                        view->UpdatePlayback(preview->Playback());
                }
            }

        private:
            void AddNode(GraphNodeKind kind)
            {
                const auto &m     = GetDefaultUiTheme().metrics;
                const auto  index = document->Source().definition.nodes.size();
                const float scale = GetDefaultUiTheme().scale;
                const auto  id =
                    document->AddNode(kind, float(index % 3) * (m.fieldWidth + m.controlPad) / scale, float(index / 3) * m.rowHeight * 5 / scale);
                view->Sync();
                view->Select({id});
            }
            void AddAction(std::string           name,
                           std::string           label,
                           std::function<void()> callback,
                           std::string           key   = {},
                           std::string           icon  = {},
                           EditorGroup           group = EditorGroup::None)
            {
                const auto                id    = "animation.graph." + name;
                const std::weak_ptr<void> alive = lifetime;
                EditorAction              action;
                action.moduleLease         = preview ? preview->CodeLease() : nullptr;
                action.id                  = id;
                action.label               = std::move(label);
                action.icon                = std::move(icon);
                action.group               = group;
                action.menu                = EditorMenu::Edit;
                action.submenu             = "Graph";
                action.scope               = scope;
                action.accelerator         = std::move(key);
                action.enabled             = [this, alive] { return !alive.expired() && document->IsLoaded() && view && !view->Editing(); };
                action.acceleratorEligible = [this, alive] { return !alive.expired() && view && view->CanvasFocused(); };
                action.invoke              = [alive, code = preview, callback = std::move(callback)] {
                    if (!alive.expired())
                        callback();
                };
                EditorActionRegistry::Get()->Add(std::move(action));
                actionIds.push_back(id);
            }
            std::unique_ptr<GraphDocument>           document;
            std::shared_ptr<CharacterPreview>        preview;
            std::unique_ptr<CharacterPreviewActions> previewActions;
            GraphEditorView                         *view     = nullptr;
            std::shared_ptr<void>                    lifetime = std::make_shared<int>(0);
            std::vector<std::string>                 actionIds;
            std::string                              scope = "assetViewer";
        };
    } // namespace
    std::unique_ptr<IAssetViewDocument> CreateGraphEditor(std::unique_ptr<GraphDocument> document, std::shared_ptr<CharacterPreview> preview)
    {
        return document ? std::make_unique<GraphEditorDocument>(std::move(document), std::move(preview)) : nullptr;
    }
} // namespace sky::animation::editor
