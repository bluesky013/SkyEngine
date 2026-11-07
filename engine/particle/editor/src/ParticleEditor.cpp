#include <particle/editor/ParticleEditor.h>

#include <core/file/FileSystem.h>
#include <core/file/FileUtil.h>
#include <core/logger/Logger.h>
#include <editor/core/EditorCore.h>
#include <editor/core/asset/AssetCookService.h>
#include <editor/core/asset/AssetPreviewProvider.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/UiSkin.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/World.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <particle/ParticleSolver.h>
#include <particle/ParticleSchema.h>
#include <particle/assets/ParticleEffectSource.h>
#include <particle/assets/ParticleEffectAsset.h>
#include <particle/assets/ParticlePreviewEffectRegistry.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace sky::particle::editor {
    namespace {
        constexpr const char *TAG = "ParticleEditor";

        uint32_t PackColor(const Color &color)
        {
            const auto channel = [](float value) { return uint32_t(std::clamp(value, 0.f, 1.f) * 255.f + .5f); };
            return sky::editor::uidraw::RGB(channel(color.r), channel(color.g), channel(color.b), channel(color.a));
        }
    } // namespace

    ParticleSourceStorage ParticleEffectStorage(const Uuid &asset)
    {
        return {[asset](std::string &out, std::string &error) -> bool {
                    std::string path;
                    if (!AssetDataBase::Get()->GetAbsoluteSourcePath(asset, path)) {
                        error = "Particle source is not registered";
                        return false;
                    }
                    std::ifstream stream(PathFromUtf8(path), std::ios::binary | std::ios::ate);
                    if (!stream) {
                        error = "Cannot open Particle source";
                        return false;
                    }
                    const auto size = stream.tellg();
                    if (size <= 0 || static_cast<size_t>(size) > MAX_PARTICLE_SOURCE_BYTES) {
                        error = "Particle source size is invalid";
                        return false;
                    }
                    std::string text(static_cast<size_t>(size), '\0');
                    stream.seekg(0);
                    if (!stream.read(text.data(), size)) {
                        error = "Cannot read complete Particle source";
                        return false;
                    }
                    out = std::move(text);
                    return true;
                },
                [asset](std::string_view text, std::string &error) -> bool {
                    std::string path;
                    if (!AssetDataBase::Get()->GetAbsoluteSourcePath(asset, path)) {
                        error = "Particle source is not registered";
                        return false;
                    }
                    const FilePath   target(path);
                    NativeFileSystem fs(target.Parent());
                    const FilePath   staging(target.FileName() + "." + Uuid::Create().ToString() + ".tmp");
                    auto             status = fs.WriteFileExclusive(staging, {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
                    if (status)
                        status = fs.ReplaceFileAtomic(staging, FilePath(target.FileName()));
                    if (!status) {
                        std::error_code ignored;
                        std::filesystem::remove(PathFromUtf8((target.Parent() / staging).GetStr()), ignored);
                        error = status.message;
                        return false;
                    }
                    return true;
                },
                [](const Uuid &id) { sky::editor::AssetCookService::Get()->RequestCook(id); }};
    }

    class ParticleDocument::Edit final : public sky::editor::UndoCommand {
    public:
        Edit(ParticleDocument &document, std::string label, ParticleEffectDesc next)
            : UndoCommand(std::move(label)), document(document), before(document.Effect()), after(std::move(next))
        {
        }
        void Do() override
        {
            document.SetEffect(after);
        }
        void Undo() override
        {
            document.SetEffect(before);
        }

    private:
        ParticleDocument  &document;
        ParticleEffectDesc before, after;
    };

    ParticleDocument::ParticleDocument(Uuid inAsset, ParticleSourceStorage inStorage)
        : Document(inAsset.ToString()), asset(inAsset), storage(std::move(inStorage))
    {
    }
    ParticleDocument::ParticleDocument(Uuid inAsset) : ParticleDocument(inAsset, ParticleEffectStorage(inAsset))
    {
    }
    ParticleDocument::~ParticleDocument()
    {
        commands.Clear();
    }
    bool ParticleDocument::Load()
    {
        std::string text, error;
        if (!storage.read || !storage.read(text, error)) {
            lastError = error.empty() ? "Particle source reader unavailable" : error;
            return false;
        }
        ParticleEffectDesc next;
        if (!ParseParticleSource(text, next, error)) {
            lastError = error;
            return false;
        }
        auto nextCompiled = CompileParticleEffect(next);
        if (!nextCompiled.succeeded) {
            lastError = nextCompiled.diagnostics.empty() ? "Particle compilation failed" : nextCompiled.diagnostics.front().message;
            return false;
        }
        commands.Clear();
        effect   = std::move(next);
        compiled = std::move(nextCompiled);
        loaded = true;
        if (!WriteParticleSource(effect, saved)) {
            lastError = "Particle source cannot be serialized";
            loaded    = false;
            return false;
        }
        savePending = false;
        lastError.clear();
        ++revision;
        ClearDirty();
        return true;
    }
    bool ParticleDocument::Save()
    {
        savePending = false;
        if (!loaded) {
            if (lastError.empty())
                lastError = "Particle source must load successfully before saving";
            return false;
        }
        compiled = CompileParticleEffect(effect);
        if (!compiled.succeeded) {
            lastError = compiled.diagnostics.empty() ? "Particle compilation failed" : compiled.diagnostics.front().message;
            return false;
        }
        std::string text, error;
        if (!WriteParticleSource(effect, text)) {
            lastError = "Particle source cannot be serialized";
            return false;
        }
        if (!storage.write || !storage.write(text, error)) {
            lastError = error.empty() ? "Particle source writer unavailable" : error;
            return false;
        }
        saved = std::move(text);
        lastError.clear();
        ++revision;
        ClearDirty();
        if (storage.requestCook)
            storage.requestCook(asset);
        return true;
    }
    void ParticleDocument::RequestSave()
    {
        savePending = true;
    }
    bool ParticleDocument::FlushPendingSave()
    {
        if (!savePending)
            return true;
        savePending = false;
        return Save();
    }
    bool ParticleDocument::IsSavePending() const
    {
        return savePending;
    }
    bool ParticleDocument::IsLoaded() const
    {
        return loaded;
    }
    const ParticleEffectDesc &ParticleDocument::Effect() const
    {
        return effect;
    }
    ParticleEffectDesc &ParticleDocument::Effect()
    {
        return effect;
    }
    const ParticleCompileResult &ParticleDocument::Compiled() const
    {
        return compiled;
    }
    const std::string &ParticleDocument::LastError() const
    {
        return lastError;
    }
    uint64_t ParticleDocument::Revision() const
    {
        return revision;
    }
    const Uuid &ParticleDocument::AssetId() const
    {
        return asset;
    }
    sky::editor::CommandService &ParticleDocument::Commands()
    {
        return commands;
    }
    void ParticleDocument::MarkEdited()
    {
        compiled = CompileParticleEffect(effect);
        if (!compiled.succeeded)
            lastError = compiled.diagnostics.empty() ? "Particle compilation failed" : compiled.diagnostics.front().message;
        else
            lastError.clear();
        std::string encoded;
        if (WriteParticleSource(effect, encoded) && encoded == saved)
            ClearDirty();
        else
            MarkDirty();
        ++revision;
    }
    void ParticleDocument::SetEffect(const ParticleEffectDesc &next)
    {
        effect = next;
        MarkEdited();
    }
    sky::editor::UndoCommandPtr ParticleDocument::MakeEdit(std::string label, ParticleEffectDesc next)
    {
        return std::make_unique<Edit>(*this, std::move(label), std::move(next));
    }
    bool ParticleDocument::Apply(std::string label, ParticleEffectDesc next)
    {
        return commands.Execute(MakeEdit(std::move(label), std::move(next)));
    }
    void ParticleDocument::ReplaceEffect(ParticleEffectDesc next)
    {
        Apply("Edit Particle effect", std::move(next));
    }

    class ParticlePreviewSession {
    public:
        class Preview;
        class RenderPreview;

        explicit ParticlePreviewSession(ParticleDocument &document);
        ~ParticlePreviewSession();
        void                         Tick(float delta);
        void                         Resize(uint32_t width, uint32_t height);
        void                         Paint(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, sky::ui::UITextSystem *text) const;
        sky::ui::UIEventResult       Pointer(const sky::ui::UIPointerEvent &event, const sky::ui::UIRect &rect);
        bool                         WantsPointerCapture() const;
        const std::string           &Diagnostic() const;

    private:
        void SyncEffect();

        ParticleDocument              &document;
        std::unique_ptr<Preview>       preview;
        std::unique_ptr<RenderPreview> render;
        uint64_t                       revision     = 0;
        float                          lastPointerX = 0.f, lastPointerY = 0.f;
        bool                           orbiting     = false;
    };

    class ParticlePreviewSession::Preview {
    public:
        void Reset(const ParticleCompileResult &compiled)
        {
            solvers.clear();
            if (!compiled.succeeded)
                return;
            for (const auto &program : compiled.programs) {
                ParticleSolver solver;
                if (solver.Init(program))
                    solvers.push_back(std::move(solver));
            }
        }
        void Tick(float delta)
        {
            const auto start = std::chrono::steady_clock::now();
            for (auto &solver : solvers)
                solver.Step(std::max(0.f, delta));
            simMilliseconds = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
        uint32_t Alive() const
        {
            uint32_t count = 0;
            for (const auto &solver : solvers)
                count += solver.GetStorage().aliveCount;
            return count;
        }
        float SimMilliseconds() const
        {
            return simMilliseconds;
        }
        void Paint(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, sky::ui::UITextSystem *text) const
        {
            const auto &theme = sky::editor::GetDefaultUiTheme();
            context.AddRect(rect, sky::editor::uidraw::color::Field);
            const sky::ui::UIPoint center{(rect.left + rect.right) * .5f, (rect.top + rect.bottom) * .5f};
            const float            scale = std::min(rect.Width(), rect.Height()) / 24.f;
            for (const auto &solver : solvers) {
                const auto &storage = solver.GetStorage();
                for (const auto slot : storage.alive) {
                    const auto size = std::max(theme.metrics.controlPad * .5f, storage.sizes[slot] * scale);
                    const auto x    = center.x + storage.positions[slot].x * scale;
                    const auto y    = center.y - storage.positions[slot].y * scale;
                    if (x < rect.left - size || x > rect.right + size || y < rect.top - size || y > rect.bottom + size)
                        continue;
                    context.AddRect({x - size * .5f, y - size * .5f, x + size * .5f, y + size * .5f}, PackColor(storage.colors[slot]));
                }
            }
            if (text) {
                const std::string stats = "Particles " + std::to_string(Alive()) + "  sim " + std::to_string(SimMilliseconds()) + " ms";
                sky::editor::uidraw::Text(
                    context, stats, theme.fonts.label,
                    {rect.left + theme.metrics.controlPad, rect.top + theme.metrics.controlPad, rect.right, rect.top + theme.metrics.rowHeight},
                    theme.colors.text, text);
            }
        }

    private:
        std::vector<ParticleSolver> solvers;
        float                       simMilliseconds = 0.f;
    };

    namespace {
        const TypeNode *ParticleComponentType()
        {
            const auto *node = SerializationContext::Get()->FindType("ParticleComponent");
            if (!node || !node->info)
                return nullptr;
            for (const auto &[group, types] : ComponentFactory::Get()->GetTypes())
                for (const auto &type : types)
                    if (type.typeId == node->info->registeredId)
                        return node;
            return nullptr;
        }
        bool SetParticleField(ComponentBase *component, const TypeNode &type, std::string_view name, const Any &value)
        {
            const auto field = type.members.find(name);
            return component && field != type.members.end() && field->second.info && value.Info() &&
                   field->second.info->registeredId == value.Info()->registeredId && field->second.setterFn &&
                   field->second.setterFn(component, value.Data());
        }
    } // namespace

    class ParticlePreviewSession::RenderPreview {
    public:
        bool Init(const Uuid &effect, uint32_t inWidth, uint32_t inHeight)
        {
            Close();
            effectUuid     = effect;
            width          = std::max(1u, inWidth);
            height         = std::max(1u, inHeight);
            auto *provider = sky::editor::AssetPreviewProviderRegistry::Get()->GetProvider();
            if (!provider) {
                diagnostic = "Preview renderer is unavailable";
                return false;
            }
            const auto *registration = WorldSubSystemRegistry::Get().GetRegistration(Name("Particle"));
            if (!registration) {
                diagnostic = "Particle.World provider is unavailable";
                return false;
            }
            auto next   = WorldPtr(World::CreateWorld());
            auto system = registration->factory(*next, {});
            if (!system) {
                diagnostic = "Cannot create preview Particle subsystem";
                return false;
            }
            next->AddSubSystem(Name("Particle"), system.release());
            auto character = std::make_unique<Actor>(Uuid::Create());
            character->SetName("Particle preview");
            const auto *type      = ParticleComponentType();
            auto       *component = type ? character->AddComponent(type->info->registeredId) : nullptr;
            if (!component || !SetParticleField(component, *type, "effect", Any(effect)) ||
                !SetParticleField(component, *type, "autoplay", Any(true)) || !SetParticleField(component, *type, "loop", Any(true))) {
                diagnostic = "Registered Particle component does not provide the preview property contract";
                return false;
            }
            if (!next->AttachToWorld(std::move(character))) {
                diagnostic = "Cannot attach preview particle actor";
                return false;
            }
            camera.Focus({0.f, 0.f, 0.f});
            auto created = provider->CreateSession({next, camera, width, height});
            if (const auto *error = std::get_if<sky::editor::AssetPreviewError>(&created)) {
                diagnostic = error->message;
                return false;
            }
            session      = std::get<sky::editor::AssetPreviewHandle>(created);
            world        = std::move(next);
            previewWorld = world->GetRuntimeId();
            world->StartSimulation();
            return true;
        }
        void SetEffect(std::shared_ptr<const ParticleEffectAssetData> data)
        {
            if (previewWorld)
                ParticlePreviewEffectRegistry::Get()->Set(previewWorld, effectUuid, std::move(data));
        }
        void Close()
        {
            ParticlePreviewEffectRegistry::Get()->Clear(previewWorld, effectUuid);
            previewWorld = {};
            if (world) {
                world->StopSimulation();
                if (auto *provider = sky::editor::AssetPreviewProviderRegistry::Get()->GetProvider())
                    provider->CloseSession(session);
            }
            session = {};
            world.Reset(nullptr);
            texture = 0;
            ready   = false;
        }
        void Resize(uint32_t inWidth, uint32_t inHeight)
        {
            width  = std::max(1u, inWidth);
            height = std::max(1u, inHeight);
        }
        void Tick(float delta)
        {
            if (!world)
                return;
            world->Tick(std::isfinite(delta) && delta >= 0.f ? delta : 0.f);
            auto *provider = sky::editor::AssetPreviewProviderRegistry::Get()->GetProvider();
            if (!provider) {
                diagnostic = "Preview renderer was closed";
                texture    = 0;
                ready      = false;
                return;
            }
            const auto resized = provider->Resize(session, width, height);
            const auto updated = provider->Update(session, camera);
            const auto state   = provider->Status(session);
            if (const auto *error = std::get_if<sky::editor::AssetPreviewError>(&resized))
                diagnostic = error->message;
            if (const auto *error = std::get_if<sky::editor::AssetPreviewError>(&updated))
                diagnostic = error->message;
            if (const auto *error = std::get_if<sky::editor::AssetPreviewError>(&state)) {
                diagnostic = error->message;
                texture    = 0;
                ready      = false;
            } else {
                const auto &status = std::get<sky::editor::AssetPreviewStatus>(state);
                texture            = status.texture;
                ready              = status.state == sky::editor::AssetPreviewState::Ready && texture != 0;
                if (!status.diagnostic.empty())
                    diagnostic = status.diagnostic;
            }
        }
        bool Available() const
        {
            return world != nullptr;
        }
        sky::editor::EditorCamera &Camera()
        {
            return camera;
        }
        bool Ready() const
        {
            return ready;
        }
        uint32_t Texture() const
        {
            return texture;
        }
        const std::string &Diagnostic() const
        {
            return diagnostic;
        }

    private:
        WorldPtr                        world;
        Uuid                            effectUuid;
        Uuid                            previewWorld;
        sky::editor::AssetPreviewHandle session;
        sky::editor::EditorCamera       camera;
        uint32_t                        width = 512, height = 512;
        uint32_t                        texture = 0;
        bool                            ready   = false;
        std::string                     diagnostic;
    };

    ParticlePreviewSession::ParticlePreviewSession(ParticleDocument &inDocument)
        : document(inDocument)
        , preview(std::make_unique<Preview>())
        , render(std::make_unique<RenderPreview>())
    {
        if (!render->Init(document.AssetId(), 512, 512))
            LOG_W(TAG, "render preview unavailable: %s", render->Diagnostic().c_str());
        SyncEffect();
        preview->Reset(document.Compiled());
        revision = document.Revision();
    }
    ParticlePreviewSession::~ParticlePreviewSession()
    {
        render->Close();
    }
    void ParticlePreviewSession::SyncEffect()
    {
        auto        data     = std::make_shared<ParticleEffectAssetData>();
        const auto &compiled = document.Compiled();
        data->version        = document.Effect().version;
        data->programs       = compiled.programs;
        data->valid          = compiled.succeeded;
        render->SetEffect(std::move(data));
    }
    void ParticlePreviewSession::Tick(float delta)
    {
        if (revision != document.Revision()) {
            revision = document.Revision();
            SyncEffect();
        }
        const bool useRender = render->Available();
        if (useRender)
            render->Tick(delta);
        if (!useRender || !render->Ready())
            preview->Tick(delta);
    }
    void ParticlePreviewSession::Resize(uint32_t width, uint32_t height)
    {
        render->Resize(width, height);
    }
    void ParticlePreviewSession::Paint(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, sky::ui::UITextSystem *text) const
    {
        const auto &theme = sky::editor::GetDefaultUiTheme();
        if (render->Ready() && render->Texture())
            context.AddTexturedQuad(rect, {0, 0, 1, 1}, render->Texture(), theme.colors.white);
        else
            preview->Paint(context, rect, text);
        std::string message = document.LastError();
        if (message.empty() && !render->Available())
            message = Diagnostic();
        if (!message.empty() && text)
            sky::editor::uidraw::Text(context, message, theme.fonts.label, rect, theme.colors.error, text,
                                      sky::editor::uidraw::HAlign::Center, sky::editor::uidraw::VAlign::Bottom);
    }
    sky::ui::UIEventResult ParticlePreviewSession::Pointer(const sky::ui::UIPointerEvent &event, const sky::ui::UIRect &rect)
    {
        if (event.action == sky::ui::UIPointerAction::UP && orbiting) {
            orbiting = false;
            return sky::ui::UIEventResult::HANDLED;
        }
        if (!render->Available() || !rect.Contains(event.x, event.y))
            return sky::ui::UIEventResult::UNHANDLED;
        if (event.action == sky::ui::UIPointerAction::DOWN && event.button == 0) {
            orbiting     = true;
            lastPointerX = event.x;
            lastPointerY = event.y;
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.action == sky::ui::UIPointerAction::MOVE && orbiting) {
            render->Camera().Look(event.x - lastPointerX, event.y - lastPointerY, .005f, true);
            lastPointerX = event.x;
            lastPointerY = event.y;
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.action == sky::ui::UIPointerAction::WHEEL && event.wheelDelta != 0.f) {
            render->Camera().Dolly(event.wheelDelta * .1f);
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }
    bool ParticlePreviewSession::WantsPointerCapture() const
    {
        return orbiting;
    }
    const std::string &ParticlePreviewSession::Diagnostic() const
    {
        return render->Diagnostic();
    }

    class ParticlePreviewView final : public sky::ui::UIElement {
    public:
        ParticlePreviewView(ParticlePreviewSession &session, sky::ui::UITextSystem *text) : session(session), text(text)
        {
        }
        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            session.Paint(context, GetBounds(), text);
        }
        void ArrangeChildren(const sky::ui::UIRect &content) override
        {
            session.Resize(static_cast<uint32_t>(std::max(1.f, content.Width())), static_cast<uint32_t>(std::max(1.f, content.Height())));
        }
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override
        {
            return session.Pointer(event, GetBounds());
        }
        bool WantsPointerCapture() const override
        {
            return session.WantsPointerCapture();
        }

    private:
        ParticlePreviewSession &session;
        sky::ui::UITextSystem  *text = nullptr;
    };

    class ParticleEditorView::StackView final : public sky::ui::UIElement {
    public:
        enum class RowKind : int { EFFECT, EMITTER, MODULE, RENDERER, PARAMETER };
        struct Selection {
            RowKind kind = RowKind::EFFECT;
            int     emitter = -1;
            int     module = -1;
            int     renderer = -1;
            int     parameter = -1;
        };
        using Command = std::function<void(Selection)>;
        StackView(ParticleDocument &document, Command command) : document(document), command(std::move(command))
        {
        }
        void SetSelection(int emitter, int module, int renderer, int parameter)
        {
            selectedEmitter   = emitter;
            selectedModule    = module;
            selectedRenderer  = renderer;
            selectedParameter = parameter;
            MarkPaintDirty();
        }
        void ArrangeChildren(const sky::ui::UIRect &) override
        {
            MarkPaintDirty();
        }
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override
        {
            if (!GetBounds().Contains(event.x, event.y))
                return sky::ui::UIEventResult::UNHANDLED;
            if (event.action == sky::ui::UIPointerAction::WHEEL) {
                scroll = std::clamp(scroll - event.wheelDelta * sky::editor::GetDefaultUiTheme().metrics.rowHeight,
                                    0.f, maxScroll);
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (event.action != sky::ui::UIPointerAction::DOWN)
                return sky::ui::UIEventResult::UNHANDLED;
            for (const auto &row : rows) {
                if (!row.rect.Contains(event.x, event.y))
                    continue;
                command(row.selection);
                return sky::ui::UIEventResult::HANDLED;
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }
        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            const auto               &theme = sky::editor::GetDefaultUiTheme();
            const sky::editor::UiSkin skin(theme, text);
            skin.DrawPanel(context, GetBounds(), {}, false);
            const auto content = GetBounds();
            context.PushClip(content);
            rows.clear();
            scroll                = std::clamp(scroll, 0.f, maxScroll);
            float       y         = content.top + theme.metrics.controlPad - scroll;
            const float rowHeight = theme.metrics.rowHeight;
            const float indent    = theme.metrics.indentX;
            auto        row       = [&](Selection selection, int depth, const std::string &label, bool selected) {
                sky::ui::UIRect rect{content.left + theme.metrics.controlPad + depth * indent, y, content.right - theme.metrics.controlPad,
                                     y + rowHeight};
                skin.DrawRow(context, rect, selected ? sky::editor::RowState::Selected : sky::editor::RowState::Normal);
                if (text)
                    sky::editor::uidraw::Text(context, label, theme.fonts.label,
                                              {rect.left + theme.metrics.controlPad, rect.top, rect.right, rect.bottom},
                                              selected ? theme.colors.textOnAccent : theme.colors.text, text, sky::editor::uidraw::HAlign::Left,
                                              sky::editor::uidraw::VAlign::Middle, true);
                rows.push_back({selection, rect});
                y += rowHeight;
            };
            const auto group = [&](int depth, const std::string &label) {
                if (!text)
                    return;
                const sky::ui::UIRect rect{content.left + theme.metrics.controlPad + depth * indent, y, content.right - theme.metrics.controlPad,
                                           y + rowHeight};
                sky::editor::uidraw::Text(context, label, theme.fonts.label, {rect.left + theme.metrics.controlPad, rect.top, rect.right, rect.bottom},
                                          theme.colors.textMuted, text, sky::editor::uidraw::HAlign::Left, sky::editor::uidraw::VAlign::Middle);
                y += rowHeight;
            };
            Selection effect;
            row(effect, 0, "Effect", selectedEmitter < 0);
            for (int emitter = 0; emitter < static_cast<int>(document.Effect().emitters.size()); ++emitter) {
                const auto &value = document.Effect().emitters[emitter];
                Selection   emitterSelection;
                emitterSelection.kind    = RowKind::EMITTER;
                emitterSelection.emitter = emitter;
                row(emitterSelection, 1, std::string(value.name.GetStr()),
                    selectedEmitter == emitter && selectedModule < 0 && selectedRenderer < 0);
                if (selectedEmitter != emitter)
                    continue;
                if (!value.modules.empty()) {
                    group(2, "Modules");
                    for (int module = 0; module < static_cast<int>(value.modules.size()); ++module) {
                        Selection moduleSelection;
                        moduleSelection.kind    = RowKind::MODULE;
                        moduleSelection.emitter = emitter;
                        moduleSelection.module  = module;
                        row(moduleSelection, 3, std::string(moduleName(value.modules[module].type)), selectedModule == module && selectedParameter < 0);
                        if (selectedModule != module)
                            continue;
                        const auto *schema = FindParticleModuleSchema(value.modules[module].type);
                        if (!schema)
                            continue;
                        for (int parameter = 0; parameter < static_cast<int>(schema->parameters.size()); ++parameter) {
                            Selection parameterSelection;
                            parameterSelection.kind      = RowKind::PARAMETER;
                            parameterSelection.emitter   = emitter;
                            parameterSelection.module    = module;
                            parameterSelection.parameter = parameter;
                            row(parameterSelection, 4, std::string(schema->parameters[parameter].name), selectedParameter == parameter);
                        }
                    }
                }
                if (!value.renderers.empty()) {
                    group(2, "Renderers");
                    for (int renderer = 0; renderer < static_cast<int>(value.renderers.size()); ++renderer) {
                        Selection rendererSelection;
                        rendererSelection.kind     = RowKind::RENDERER;
                        rendererSelection.emitter  = emitter;
                        rendererSelection.renderer = renderer;
                        row(rendererSelection, 3, std::string(rendererName(value.renderers[renderer].type)), selectedRenderer == renderer);
                    }
                }
            }
            context.PopClip();
            maxScroll = std::max(0.f, (y - content.top - theme.metrics.controlPad) - content.Height());
        }
        void SetText(sky::ui::UITextSystem *value)
        {
            text = value;
        }

    private:
        template <class T>
        static std::string_view enumName(T value)
        {
            const auto *node = sky::GetTypeNode(TypeInfo<T>::RegisteredId());
            if (!node)
                return {};
            const auto found = node->enums.find(static_cast<uint64_t>(value));
            return found == node->enums.end() ? std::string_view{} : found->second;
        }
        static std::string_view moduleName(ParticleModuleType type)
        {
            return enumName(type);
        }
        static std::string_view rendererName(ParticleRendererType type)
        {
            return enumName(type);
        }
        struct Row {
            Selection       selection;
            sky::ui::UIRect rect;
        };
        ParticleDocument      &document;
        Command                command;
        sky::ui::UITextSystem *text = nullptr;
        std::vector<Row>       rows;
        float                  scroll            = 0.f;
        float                  maxScroll         = 0.f;
        int                    selectedEmitter   = -1;
        int                    selectedModule    = -1;
        int                    selectedRenderer  = -1;
        int                    selectedParameter = -1;
    };

    ParticleEditorView::ParticleEditorView(ParticleDocument &inDocument, sky::ui::UITextSystem *inText, std::string scope)
        : document(inDocument)
        , text(inText)
        , actionScope(std::move(scope))
    {
        const sky::editor::ReflectedFormContext context{document.Commands(), sky::editor::EditorCore::GetPropertyEditors(),
                                                        sky::editor::EditorCore::GetPropertyChanges(), sky::editor::GetDefaultUiTheme()};
        auto stackView = std::make_unique<StackView>(document, [this](StackView::Selection selection) {
            selectedEmitter   = selection.emitter;
            selectedModule    = selection.module;
            selectedRenderer  = selection.renderer;
            selectedParameter = selection.parameter;
            BindSelection();
        });
        stack          = static_cast<StackView *>(AddChild(std::move(stackView)));
        stack->SetText(text);
        form = static_cast<sky::editor::ReflectedFormView *>(
            AddChild(std::make_unique<sky::editor::ReflectedFormView>(text, "Selected Particle Object", context)));
        form->Form().SetEditCommandFactory([this](const sky::editor::PropertyDescriptor &field, Any value) {
            if (!field.SetValue(value))
                return sky::editor::UndoCommandPtr{};
            return document.MakeEdit("Edit Particle property", editSnapshot);
        });
        form->SetOnEdited([this] { MarkPaintDirty(); });
        beforeHistory = document.Commands().AddBeforeHistoryCallback([this] { Cancel(); });
        if (!actionScope.empty()) {
            const auto addAction = [&](std::string id, std::string label, std::string icon, int order, std::function<void()> invoke) {
                sky::editor::EditorAction action;
                action.id      = std::move(id);
                action.label   = std::move(label);
                action.icon    = std::move(icon);
                action.group   = sky::editor::EditorGroup::History;
                action.menu    = sky::editor::EditorMenu::Edit;
                action.submenu = "Particle";
                action.scope   = actionScope;
                action.order   = order;
                action.enabled = [this] { return !Editing(); };
                action.invoke  = std::move(invoke);
                sky::editor::EditorActionRegistry::Get()->Add(std::move(action));
            };
            addAction("particle.addEmitter", "Add emitter", "speed-up", 30, [this] { AddEmitter(); });
            addAction("particle.addModule", "Add module", "speed-up", 31, [this] { AddModule(); });
            {
                sky::editor::EditorAction action;
                action.id      = "particle.addRenderer";
                action.label   = "Add renderer";
                action.icon    = "speed-up";
                action.group   = sky::editor::EditorGroup::History;
                action.menu    = sky::editor::EditorMenu::Edit;
                action.submenu = "Particle";
                action.scope   = actionScope;
                action.order   = 35;
                action.enabled = [this] {
                    return !Editing() && selectedEmitter >= 0 && selectedEmitter < static_cast<int>(document.Effect().emitters.size());
                };
                action.invoke = [this] { AddRenderer(); };
                sky::editor::EditorActionRegistry::Get()->Add(std::move(action));
            }
            addAction("particle.remove", "Remove selection", "speed-down", 32, [this] { RemoveSelection(); });
            addAction("particle.moveUp", "Move module up", "translate", 33, [this] { MoveModule(-1); });
            addAction("particle.moveDown", "Move module down", "translate", 34, [this] { MoveModule(1); });
        }
        BindSelection();
    }

    void ParticleEditorView::AddEmitter()
    {
        auto                next = document.Effect();
        ParticleEmitterDesc emitter;
        emitter.name       = Name(("Emitter " + std::to_string(next.emitters.size() + 1)).c_str());
        emitter.capacity   = 128;
        emitter.spawn.rate = 10.f;
        emitter.modules.push_back({ParticleModuleType::INITIALIZE, Name("Initialize"), {}});
        emitter.renderers.push_back({});
        next.emitters.push_back(std::move(emitter));
        document.Apply("Add Particle emitter", std::move(next));
        selectedEmitter = static_cast<int>(document.Effect().emitters.size()) - 1;
        selectedModule = selectedRenderer = -1;
    }
    void ParticleEditorView::AddRenderer()
    {
        if (selectedEmitter < 0 || selectedEmitter >= static_cast<int>(document.Effect().emitters.size()))
            return;
        auto                  next = document.Effect();
        ParticleRendererDesc renderer;
        next.emitters[selectedEmitter].renderers.push_back(renderer);
        selectedRenderer = static_cast<int>(next.emitters[selectedEmitter].renderers.size()) - 1;
        selectedModule   = -1;
        document.Apply("Add Particle renderer", std::move(next));
    }
    void ParticleEditorView::AddModule()
    {
        auto next = document.Effect();
        if (selectedEmitter < 0 || selectedEmitter >= static_cast<int>(next.emitters.size()))
            return;
        auto &modules = next.emitters[selectedEmitter].modules;
        modules.push_back({ParticleModuleType::INITIALIZE, Name("Module"), {}});
        selectedModule   = static_cast<int>(modules.size()) - 1;
        selectedRenderer = -1;
        document.Apply("Add Particle module", std::move(next));
    }
    void ParticleEditorView::RemoveSelection()
    {
        auto next = document.Effect();
        if (selectedEmitter < 0 || selectedEmitter >= static_cast<int>(next.emitters.size()))
            return;
        auto &emitter = next.emitters[selectedEmitter];
        if (selectedModule >= 0 && selectedModule < static_cast<int>(emitter.modules.size()))
            emitter.modules.erase(emitter.modules.begin() + selectedModule);
        else if (selectedRenderer >= 0 && selectedRenderer < static_cast<int>(emitter.renderers.size()))
            emitter.renderers.erase(emitter.renderers.begin() + selectedRenderer);
        else if (next.emitters.size() > 1)
            next.emitters.erase(next.emitters.begin() + selectedEmitter);
        else
            return;
        document.Apply("Remove Particle selection", std::move(next));
        selectedEmitter = std::min(selectedEmitter, static_cast<int>(document.Effect().emitters.size()) - 1);
        selectedModule = selectedRenderer = -1;
    }
    void ParticleEditorView::MoveModule(int direction)
    {
        auto next = document.Effect();
        if (selectedEmitter < 0 || selectedEmitter >= static_cast<int>(next.emitters.size()))
            return;
        auto     &modules = next.emitters[selectedEmitter].modules;
        const int target  = selectedModule + direction;
        if (selectedModule < 0 || target < 0 || selectedModule >= static_cast<int>(modules.size()) || target >= static_cast<int>(modules.size()))
            return;
        std::swap(modules[selectedModule], modules[target]);
        document.Apply("Move Particle module", std::move(next));
        selectedModule = target;
    }

    void ParticleEditorView::BindSelection()
    {
        if (!form)
            return;
        form->CancelInteractions();
        editSnapshot = document.Effect();
        if (selectedEmitter < 0 || selectedEmitter >= static_cast<int>(editSnapshot.emitters.size()))
            selectedEmitter = selectedModule = selectedRenderer = selectedParameter = -1;
        if (selectedModule < 0 || selectedModule >= static_cast<int>(editSnapshot.emitters[selectedEmitter].modules.size()))
            selectedModule = selectedRenderer = selectedParameter = -1;
        if (selectedRenderer < 0 || selectedRenderer >= static_cast<int>(editSnapshot.emitters[selectedEmitter].renderers.size()))
            selectedRenderer = selectedParameter = -1;
        const auto *schema = selectedModule >= 0 ? FindParticleModuleSchema(editSnapshot.emitters[selectedEmitter].modules[selectedModule].type) : nullptr;
        if (!schema || selectedParameter < 0 || selectedParameter >= static_cast<int>(schema->parameters.size()))
            selectedParameter = -1;
        const sky::editor::PropertyObject object = [&]() -> sky::editor::PropertyObject {
            auto &effect = editSnapshot;
            if (selectedEmitter >= 0 && selectedEmitter < static_cast<int>(effect.emitters.size())) {
                auto       &emitter = effect.emitters[selectedEmitter];
                const auto &module  = selectedModule;
                if (module >= 0 && module < static_cast<int>(emitter.modules.size())) {
                    if (selectedParameter >= 0 && schema) {
                        auto      &parameters = emitter.modules[module].parameters;
                        const auto name       = schema->parameters[selectedParameter].name;
                        auto       found = std::find_if(parameters.begin(), parameters.end(), [&](const auto &parameter) {
                            return parameter.name.GetStr() == name;
                        });
                        if (found == parameters.end()) {
                            ParticleParameterDesc parameter;
                            parameter.name = Name(std::string(name).c_str());
                            parameters.push_back(std::move(parameter));
                            found = std::prev(parameters.end());
                        }
                        return {&found->value, sky::GetTypeNode(TypeInfo<ParticleValue>::RegisteredId())};
                    }
                    return {&emitter.modules[module], sky::GetTypeNode(TypeInfo<ParticleModuleDesc>::RegisteredId())};
                }
                if (selectedRenderer >= 0 && selectedRenderer < static_cast<int>(emitter.renderers.size()))
                    return {&emitter.renderers[selectedRenderer], sky::GetTypeNode(TypeInfo<ParticleRendererDesc>::RegisteredId())};
                return {&emitter, sky::GetTypeNode(TypeInfo<ParticleEmitterDesc>::RegisteredId())};
            }
            return {&effect, sky::GetTypeNode(TypeInfo<ParticleEffectDesc>::RegisteredId())};
        }();
        form->Bind(object);
        if (stack)
            stack->SetSelection(selectedEmitter, selectedModule, selectedRenderer, selectedParameter);
        error = document.LastError();
        MarkPaintDirty();
    }
    ParticleEditorView::~ParticleEditorView()
    {
        document.Commands().RemoveBeforeHistoryCallback(beforeHistory);
        for (const auto *id : {"particle.addEmitter", "particle.addModule", "particle.addRenderer", "particle.remove", "particle.moveUp",
                               "particle.moveDown"})
            if (!actionScope.empty())
                sky::editor::EditorActionRegistry::Get()->Remove(actionScope, id);
    }
    void ParticleEditorView::OnPaint(sky::ui::UIPaintContext &context)
    {
        context.AddRect(GetBounds(), sky::editor::GetDefaultUiTheme().colors.panel);
    }
    sky::ui::UISize ParticleEditorView::OnMeasure(const sky::ui::UIConstraints &constraints)
    {
        sky::ui::UISize size = constraints.maxSize;
        if (!std::isfinite(size.width))
            size.width = 320.f;
        if (!std::isfinite(size.height))
            size.height = 240.f;
        return size;
    }
    void ParticleEditorView::ArrangeChildren(const sky::ui::UIRect &content)
    {
        if (!form)
            return;
        const auto           &theme = sky::editor::GetDefaultUiTheme();
        const float           pad   = theme.metrics.controlPad;
        const sky::ui::UIRect inner{content.left + pad, content.top + pad, content.right - pad, content.bottom - pad};
        const auto            stackBottom = inner.top + inner.Height() * .4f;
        if (stack)
            stack->Arrange({inner.left, inner.top, inner.right, stackBottom});
        form->Arrange({inner.left, stackBottom + pad, inner.right, inner.bottom});
    }
    sky::ui::UIEventResult ParticleEditorView::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (form && (form->WantsPointerCapture() || form->GetBounds().Contains(event.x, event.y))) {
            const auto result = form->OnPointerEvent(event);
            if (result == sky::ui::UIEventResult::HANDLED || form->WantsPointerCapture())
                return result;
        }
        if (stack && stack->GetBounds().Contains(event.x, event.y))
            return stack->OnPointerEvent(event);
        return sky::ui::UIEventResult::UNHANDLED;
    }
    bool ParticleEditorView::WantsPointerCapture() const
    {
        return form && form->WantsPointerCapture();
    }
    sky::ui::UIEventResult ParticleEditorView::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        return form && form->OnKeyEvent(event) == sky::ui::UIEventResult::HANDLED ? sky::ui::UIEventResult::HANDLED
                                                                                  : sky::ui::UIEventResult::UNHANDLED;
    }
    sky::ui::UIEventResult ParticleEditorView::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        return form && form->OnTextInput(event) == sky::ui::UIEventResult::HANDLED ? sky::ui::UIEventResult::HANDLED
                                                                                   : sky::ui::UIEventResult::UNHANDLED;
    }
    void ParticleEditorView::OnPointerCancel(uint32_t pointer)
    {
        if (form)
            form->OnPointerCancel(pointer);
    }
    void ParticleEditorView::OnPointerLeave(const sky::ui::UIPointerEvent &event)
    {
        if (form)
            form->OnPointerLeave(event);
    }
    void ParticleEditorView::Tick(float)
    {
        if (boundRevision != document.Revision() && !Editing()) {
            BindSelection();
            boundRevision = document.Revision();
        }
    }
    bool ParticleEditorView::Editing() const
    {
        return form && form->HasActiveInteraction();
    }
    void ParticleEditorView::Cancel()
    {
        if (form)
            form->CancelInteractions();
    }
    sky::editor::CommandService &ParticleEditorView::History()
    {
        return document.Commands();
    }
    const std::string &ParticleEditorView::Error() const
    {
        return error;
    }

    namespace {
        class ParticleEditorDocument final : public sky::editor::IAssetViewDocument {
        public:
            explicit ParticleEditorDocument(std::unique_ptr<ParticleDocument> document) : document(std::move(document))
            {
            }
            void SetActionScope(std::string scope) override
            {
                actionScope = std::move(scope);
            }
            bool FitsContentToViewport() const override
            {
                return true;
            }
            std::unique_ptr<sky::ui::UIElement> CreateContent(sky::ui::UITextSystem *text) override
            {
                auto content = std::make_unique<ParticleEditorView>(*document, text, actionScope);
                view         = content.get();
                return content;
            }
            std::unique_ptr<sky::ui::UIElement> CreatePreview(sky::ui::UITextSystem *text) override
            {
                if (!preview)
                    preview = std::make_unique<ParticlePreviewSession>(*document);
                return std::make_unique<ParticlePreviewView>(*preview, text);
            }
            sky::editor::CommandService &History() override
            {
                return document->Commands();
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
                const auto &compiled = document->Compiled();
                if (!compiled.succeeded && !compiled.diagnostics.empty())
                    return status + compiled.diagnostics.front().message;
                return status + "Particle source is valid";
            }
            void Tick(float delta) override
            {
                if (!IsEditing())
                    document->FlushPendingSave();
                if (view)
                    view->Tick(delta);
                if (preview)
                    preview->Tick(delta);
            }

        private:
            std::unique_ptr<ParticleDocument>       document;
            std::unique_ptr<ParticlePreviewSession> preview;
            std::string                             actionScope;
            ParticleEditorView                     *view = nullptr;
        };

        class ParticleViewProvider final : public sky::editor::IAssetViewProvider {
        public:
            bool UsesDocumentWindow() const override
            {
                return true;
            }
            sky::editor::AssetViewDocument OpenDocument(const Uuid &asset, const std::string &type) override
            {
                if (type != "ParticleEffect")
                    return {};
                auto document = std::make_unique<ParticleDocument>(asset);
                if (!document->Load()) {
                    LOG_W(TAG, "particle document %s load failed: %s", asset.ToString().c_str(), document->LastError().c_str());
                    return {};
                }
                return sky::editor::AssetViewDocument{std::make_unique<ParticleEditorDocument>(std::move(document))};
            }
        };
    } // namespace

    std::unique_ptr<sky::editor::IAssetViewDocument> CreateParticleEditorDocument(const Uuid &asset, const std::string &type)
    {
        ParticleViewProvider provider;
        auto                 opened = provider.OpenDocument(asset, type);
        return std::move(opened.document);
    }
    std::unique_ptr<sky::editor::IAssetViewProvider> CreateParticleViewProvider()
    {
        return std::make_unique<ParticleViewProvider>();
    }
} // namespace sky::particle::editor
