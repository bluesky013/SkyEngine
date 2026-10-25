//
// Material / material-instance property editor (custom asset-view content). See MaterialEditorView.h.
//

#include <aurora/editor/MaterialEditorView.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/adaptor/assets/MaterialReflection.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <core/archive/MemoryStreamArchive.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/JsonArchive.h>

#include <core/logger/Logger.h>
#include <editor/core/EditorCore.h>
#include <editor/core/asset/AssetCookService.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/command/CommandService.h>
#include <editor/ui/AssetViewProvider.h>
#include <editor/ui/UiDraw.h>
#include <editor/widgets/ReflectedFormView.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/GridPanel.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

static const char *TAG = "MaterialEditor";

namespace sky::aurora::editor {

    namespace uc = sky::editor::uidraw;

    namespace {

        template <typename T>
        T ReadPod(const std::vector<uint8_t> &bytes, size_t offset = 0)
        {
            T value{};
            if (bytes.size() >= offset + sizeof(T)) {
                std::memcpy(&value, bytes.data() + offset, sizeof(T));
            }
            return value;
        }

        std::string FloatToString(float v)
        {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%g", v);
            return buf;
        }

        std::string EditableText(const MaterialEditorRow &r)
        {
            if (r.isString) {
                return r.text;
            }
            return std::visit(
                [](const auto &v) -> std::string {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, float>) {
                        return FloatToString(v);
                    } else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>) {
                        return std::to_string(v);
                    } else if constexpr (std::is_same_v<T, bool>) {
                        return v ? "true" : "false";
                    } else if constexpr (std::is_same_v<T, Uuid>) {
                        return {}; // textures use DisplayText
                    } else if constexpr (std::is_same_v<T, Matrix4>) {
                        return "(matrix)";
                    } else if constexpr (std::is_same_v<T, Sampler::Descriptor>) {
                        return "(sampler settings below)";
                    } else if constexpr (std::is_same_v<T, Vector2> || std::is_same_v<T, Vector3> || std::is_same_v<T, Vector4>) {
                        std::string           s;
                        constexpr std::size_t n = sizeof(v.v) / sizeof(float);
                        for (std::size_t i = 0; i < n; ++i) {
                            if (i != 0) {
                                s += ", ";
                            }
                            s += FloatToString(v.v[i]);
                        }
                        return s;
                    } else {
                        return "(unsupported value)";
                    }
                },
                r.value);
        }

        std::string DisplayText(const MaterialEditorRow &r)
        {
            if (!r.isString && !MaterialPropertyValueMatches(r.type, r.value)) {
                return "(invalid value type)";
            }
            if (IsTextureProperty(r.type)) {
                const Uuid *texture = std::get_if<Uuid>(&r.value);
                if (texture == nullptr || !*texture) {
                    return "(none)";
                }
                std::string name;
                if (auto *catalog = sky::editor::GetEditorAssetCatalog(); catalog != nullptr && catalog->GetName(*texture, name)) {
                    return name;
                }
                return texture->ToString();
            }
            return EditableText(r);
        }

        bool ParseFloats(const std::string &text, std::vector<float> &out, size_t expected)
        {
            out.clear();
            std::stringstream ss(text);
            std::string       item;
            while (std::getline(ss, item, ',')) {
                out.push_back(std::strtof(item.c_str(), nullptr));
            }
            return out.size() == expected;
        }

        std::string SourceAbsolutePath(const Uuid &uuid)
        {
            auto *db = AssetDataBase::Get();
            if (db == nullptr) {
                return {};
            }
            std::string abs;
            if (!db->GetAbsoluteSourcePath(uuid, abs)) {
                return {};
            }
            return abs;
        }

        // Source I/O through the framework reflection (no bespoke parser): the `.mat` / `.mati`
        // source IS the reflected MaterialAssetData / MaterialInstanceSource JSON.
        template <typename T>
        bool LoadSourceJson(const Uuid &asset, T &out)
        {
            const std::string abs = SourceAbsolutePath(asset);
            if (abs.empty()) {
                return false;
            }
            std::ifstream in(abs, std::ios::binary);
            if (!in) {
                return false;
            }
            std::stringstream ss;
            ss << in.rdbuf();
            const std::string     json = ss.str();
            sky::IMemoryArchive   memory(json.data(), json.size());
            sky::JsonInputArchive archive(memory);
            archive.LoadValueObject(out);
            return true;
        }

        template <typename T>
        bool SaveSourceJson(const Uuid &asset, const T &value)
        {
            const std::string abs = SourceAbsolutePath(asset);
            if (abs.empty()) {
                return false;
            }
            sky::OMemoryArchive    memory;
            sky::JsonOutputArchive archive(memory);
            archive.SaveValueObject(value);
            std::ofstream out(abs, std::ios::binary | std::ios::trunc);
            if (!out) {
                return false;
            }
            out.write(static_cast<const char *>(memory.Data()), static_cast<std::streamsize>(memory.Size()));
            return out.good();
        }

        // History owns values; a closed or rebound view invalidates every command's weak generation.
        class MaterialValueEditCommand final : public sky::editor::UndoCommand {
        public:
            MaterialValueEditCommand(std::weak_ptr<void>                                      generation,
                                     MaterialPropertyValue                                    before,
                                     MaterialPropertyValue                                    after,
                                     bool                                                     beforeOverride,
                                     std::function<void(const MaterialPropertyValue &, bool)> apply)
                : UndoCommand("Edit material value")
                , generation(std::move(generation))
                , before(before)
                , after(after)
                , beforeOverride(beforeOverride)
                , apply(std::move(apply))
            {
            }
            void Do() override
            {
                if (!generation.expired()) {
                    apply(after, true);
                }
            }
            void Undo() override
            {
                if (!generation.expired()) {
                    apply(before, beforeOverride);
                }
            }

        private:
            std::weak_ptr<void>                                      generation;
            MaterialPropertyValue                                    before;
            MaterialPropertyValue                                    after;
            bool                                                     beforeOverride;
            std::function<void(const MaterialPropertyValue &, bool)> apply;
        };

    } // namespace

    MaterialEditorView::MaterialEditorView(sky::ui::UITextSystem       *textSystem,
                                           const Uuid                  &uuid,
                                           std::string                  titleText,
                                           bool                         instance,
                                           bool                         enableTechniques,
                                           sky::editor::CommandService *commands)
        : text(textSystem)
        , skin(sky::editor::GetDefaultUiTheme(), textSystem)
        , asset(uuid)
        , title(std::move(titleText))
        , isInstance(instance)
        , allowTechniques(enableTechniques)
        , history(commands)
    {
        SetFocusable(true);
        rowsPanel = static_cast<sky::ui::GridPanel *>(AddChild(std::make_unique<sky::ui::GridPanel>()));
        if (isInstance) {
            LoadSourceJson(asset, instanceSource);
        } else {
            LoadSourceJson(asset, materialSource);
        }
        MarkLayoutDirty();
    }

    MaterialEditorView::~MaterialEditorView()
    {
        ClearPropertyForms();
    }

    namespace {

        void AddRow(std::vector<MaterialEditorRow> &rows,
                    const std::string              &name,
                    const std::string              &text,
                    int                             techIndex,
                    int                             field,
                    const std::string              &renderKey = {})
        {
            MaterialEditorRow r;
            r.name           = name;
            r.text           = text;
            r.isString       = true;
            r.techniqueIndex = techIndex;
            r.techniqueField = field;
            r.renderKey      = renderKey;
            rows.push_back(std::move(r));
        }

        const char *CompareOpName(CompareOp op)
        {
            static const std::pair<CompareOp, const char *> kTable[] = {{CompareOp::NEVER, "never"},
                                                                        {CompareOp::LESS, "less"},
                                                                        {CompareOp::EQUAL, "equal"},
                                                                        {CompareOp::LESS_OR_EQUAL, "less_equal"},
                                                                        {CompareOp::GREATER, "greater"},
                                                                        {CompareOp::NOT_EQUAL, "not_equal"},
                                                                        {CompareOp::GREATER_OR_EQUAL, "greater_equal"},
                                                                        {CompareOp::ALWAYS, "always"}};
            for (const auto &e : kTable) {
                if (e.first == op) {
                    return e.second;
                }
            }
            return "less_equal";
        }

        const char *PolygonModeName(PolygonMode m)
        {
            if (m == PolygonMode::LINE) {
                return "line";
            }
            if (m == PolygonMode::POINT) {
                return "point";
            }
            return "fill";
        }

        std::string CullModeText(CullingModeFlags c)
        {
            const bool front = c == CullingModeFlags(CullModeFlagBits::FRONT);
            const bool back  = c == CullingModeFlags(CullModeFlagBits::BACK);
            if (front && back) {
                return "both";
            }
            if (front) {
                return "front";
            }
            if (back) {
                return "back";
            }
            return "none";
        }

        CompareOp CompareOpFromName(const std::string &s)
        {
            static const std::pair<const char *, CompareOp> kTable[] = {{"never", CompareOp::NEVER},
                                                                        {"less", CompareOp::LESS},
                                                                        {"equal", CompareOp::EQUAL},
                                                                        {"less_equal", CompareOp::LESS_OR_EQUAL},
                                                                        {"greater", CompareOp::GREATER},
                                                                        {"not_equal", CompareOp::NOT_EQUAL},
                                                                        {"greater_equal", CompareOp::GREATER_OR_EQUAL},
                                                                        {"always", CompareOp::ALWAYS}};
            for (const auto &e : kTable) {
                if (s == e.first) {
                    return e.second;
                }
            }
            return CompareOp::LESS_OR_EQUAL;
        }

        // Set one render-state field from an edited text value; returns false for unknown keys.
        bool SetRenderStateValue(MaterialRenderState &rs, const std::string &key, const std::string &value)
        {
            const bool flag = (value == "true" || value == "1");
            const auto num  = static_cast<float>(std::strtod(value.c_str(), nullptr));
            if (key == "depthTest") {
                rs.depthStencil.depthTest = flag;
            } else if (key == "depthWrite") {
                rs.depthStencil.depthWrite = flag;
            } else if (key == "stencilTest") {
                rs.depthStencil.stencilTest = flag;
            } else if (key == "alphaToCoverage") {
                rs.alphaToCoverage = flag;
            } else if (key == "depthCompare") {
                rs.depthStencil.compareOp = CompareOpFromName(value);
            } else if (key == "cullMode") {
                CullingModeFlags c;
                if (value == "front") {
                    c = CullModeFlagBits::FRONT;
                } else if (value == "back") {
                    c = CullModeFlagBits::BACK;
                } else if (value == "both") {
                    c = CullModeFlagBits::FRONT | CullModeFlagBits::BACK;
                }
                rs.rasterState.cullMode = c;
            } else if (key == "frontFace") {
                rs.rasterState.frontFace = (value == "cw") ? FrontFace::CW : FrontFace::CCW;
            } else if (key == "polygonMode") {
                rs.rasterState.polygonMode = (value == "line") ? PolygonMode::LINE : ((value == "point") ? PolygonMode::POINT : PolygonMode::FILL);
            } else if (key == "depthBias.constant") {
                rs.rasterState.depthBiasConstantFactor = num;
                rs.rasterState.depthBiasEnable         = true;
            } else if (key == "depthBias.slope") {
                rs.rasterState.depthBiasSlopeFactor = num;
                rs.rasterState.depthBiasEnable      = true;
            } else if (key == "depthBias.clamp") {
                rs.rasterState.depthBiasClamp  = num;
                rs.rasterState.depthBiasEnable = true;
            } else {
                return false;
            }
            return true;
        }

    } // namespace

    void MaterialEditorView::ReloadTechniques()
    {
        ClearPropertyForms();
        rows.clear();

        MaterialAssetData authored = materialSource;
        schemaError.clear();
        generatedSchema = {};
        if (isInstance && !LoadSourceJson(instanceSource.material, authored))
            schemaError = "Material parent source is unavailable";
        MaterialDefinitionData definition;
        const auto             root = Platform::Get()->GetImpl() ? Platform::Get()->GetBundleFileSystem() : FileSystemPtr{};
#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
        constexpr auto schemaBackend = API::METAL;
#else
        constexpr auto schemaBackend = API::VULKAN;
#endif
        if (schemaError.empty() && root) {
            auto resources = LoadShaderResources(*root, schemaBackend, ShaderResourceMode::DEVELOPMENT);
            if (const auto *failure = std::get_if<std::string>(&resources)) {
                schemaError = *failure;
            } else {
                auto &shaders = std::get<ShaderResources>(resources);
                ResolveMaterialDefinition(authored, *shaders.sources, schemaBackend, shaders.products.Get(), definition, schemaError);
            }
        } else if (schemaError.empty()) {
            schemaError = "Shader resources are unavailable; material keys cannot be generated";
        }
        if (schemaError.empty()) {
            generatedSchema = definition.schema;
            if (!isInstance) {
                const bool migrated = std::ranges::any_of(materialSource.techniques, [](const auto &technique) {
                    return !technique.aliases.empty() || !technique.program.variant.entries.empty();
                });
                materialSource      = definition.canonical;
                dirty               = dirty || migrated;
            }
            for (const auto &key : generatedSchema.keys) {
                if (isInstance && key.kind == MaterialKeyKind::OPTION)
                    continue;
                auto property = MaterialDefaultValue(key);
                if (!property)
                    continue;
                if (const auto existing = std::ranges::find(definition.canonical.properties, key.name, &MaterialPropertyData::name);
                    existing != definition.canonical.properties.end())
                    property = *existing;
                bool overridden = !isInstance;
                if (isInstance) {
                    for (auto &value : instanceSource.properties) {
                        for (const auto &technique : authored.techniques)
                            for (const auto &alias : technique.aliases)
                                if (alias.alias == value.name)
                                    value.name = alias.target;
                        if (value.name == key.name) {
                            if (value.type != property->type || !MaterialPropertyValueMatches(value.type, value.value)) {
                                schemaError = "Instance value has an incompatible type: " + key.name;
                                break;
                            }
                            property   = value;
                            overridden = true;
                        }
                    }
                }
                MaterialEditorRow row{property->name, property->type, property->value};
                row.overridden = overridden;
                rows.push_back(std::move(row));
            }
            if (isInstance)
                for (const auto &value : instanceSource.properties) {
                    const auto key = std::ranges::find(generatedSchema.keys, value.name, &MaterialKey::name);
                    if (key == generatedSchema.keys.end() || key->kind == MaterialKeyKind::OPTION)
                        schemaError = "Instance override is unknown or names a static option: " + value.name;
                }
        }
        if (!schemaError.empty()) {
            rows.clear();
            MaterialEditorRow diagnostic;
            diagnostic.kind = MaterialRowKind::Diagnostic;
            diagnostic.name = schemaError;
            rows.push_back(std::move(diagnostic));
            LOG_W(TAG, "%s", schemaError.c_str());
        }
        if (!isInstance) {
            if (allowTechniques) {
                for (size_t i = 0; i < materialSource.techniques.size(); ++i) {
                    const MaterialTechniqueData &t  = materialSource.techniques[i];
                    const int                    ti = static_cast<int>(i);
                    MaterialEditorRow            header;
                    header.kind           = MaterialRowKind::TechniqueHeader;
                    header.techniqueIndex = ti;
                    header.name           = "Technique " + std::to_string(i) + (t.tag.empty() ? "" : " [" + t.tag + "]");
                    rows.push_back(std::move(header));
                    AddRow(rows, "tag", t.tag, ti, 0);
                    AddRow(rows, "vertex.source", t.program.vertex.relativePath, ti, 1);
                    AddRow(rows, "vertex.entry", t.program.vertex.entry, ti, 2);
                    AddRow(rows, "fragment.source", t.program.fragment.relativePath, ti, 3);
                    AddRow(rows, "fragment.entry", t.program.fragment.entry, ti, 4);
                    const MaterialRenderState &rs = t.renderState;
                    auto addBool = [&](const char *key, bool v) { AddRow(rows, std::string("rs.") + key, v ? "true" : "false", ti, 10, key); };
                    auto addStr  = [&](const char *key, const std::string &v) { AddRow(rows, std::string("rs.") + key, v, ti, 10, key); };
                    addBool("depthTest", rs.depthStencil.depthTest);
                    addBool("depthWrite", rs.depthStencil.depthWrite);
                    addBool("stencilTest", rs.depthStencil.stencilTest);
                    addStr("depthCompare", CompareOpName(rs.depthStencil.compareOp));
                    addStr("cullMode", CullModeText(rs.rasterState.cullMode));
                    addStr("frontFace", rs.rasterState.frontFace == FrontFace::CW ? "cw" : "ccw");
                    addStr("polygonMode", PolygonModeName(rs.rasterState.polygonMode));
                    addBool("alphaToCoverage", rs.alphaToCoverage);
                    addStr("depthBias.constant", FloatToString(rs.rasterState.depthBiasConstantFactor));
                    addStr("depthBias.slope", FloatToString(rs.rasterState.depthBiasSlopeFactor));
                    addStr("depthBias.clamp", FloatToString(rs.rasterState.depthBiasClamp));
                }
                MaterialEditorRow add;
                add.kind = MaterialRowKind::AddTechnique;
                add.name = "+ Add Technique";
                rows.push_back(std::move(add));
            }
        }

        RebuildRowSlots();
        RebuildPropertyForms();
        MarkPaintDirty();
    }

    void MaterialEditorView::ClearPropertyForms()
    {
        for (auto *form : propertyForms) {
            form->CancelInteractions();
            form->Unbind();
            RemoveChild(form);
        }
        propertyForms.clear();
        formNodes.clear();
        formValues.clear();
        propertyGeneration = std::make_shared<int>(0);
    }

    void MaterialEditorView::RebuildPropertyForms()
    {
        formValues.resize(rows.size());
        formNodes.resize(rows.size());
        for (size_t index = 0; index < rows.size(); ++index) {
            if (rows[index].isString || rows[index].kind != MaterialRowKind::Field)
                continue;
            formValues[index] = {rows[index].name, rows[index].type, rows[index].value};
            formNodes[index]  = MakeMaterialPropertyReflection(formValues[index]);
            if (!formNodes[index])
                continue;
            const sky::editor::ReflectedFormContext services{history ? *history : sky::editor::EditorCore::GetCommandService(),
                                                             sky::editor::EditorCore::GetPropertyEditors(),
                                                             sky::editor::EditorCore::GetPropertyChanges(), Theme()};
            auto form = std::make_unique<sky::editor::ReflectedFormView>(text, rows[index].name, services);
            form->Bind({&formValues[index], formNodes[index].get()});
            const std::weak_ptr<void> generation = propertyGeneration;
            form->Form().SetEditCommandFactory([this, index, generation](const sky::editor::PropertyDescriptor &field, Any next) {
                if (generation.expired())
                    return sky::editor::UndoCommandPtr{};
                const auto                            before = rows[index].value;
                auto                                  after  = formValues[index];
                const sky::editor::PropertyDescriptor candidate{&after, field.GetMember(), field.GetName(), field.GetCategory()};
                if (!candidate.SetValue(next) || !MaterialPropertyValueMatches(after.type, after.value))
                    return sky::editor::UndoCommandPtr{};
                const auto key = std::ranges::find(generatedSchema.keys, after.name, &MaterialKey::name);
                if (key != generatedSchema.keys.end() && key->kind == MaterialKeyKind::OPTION) {
                    ShaderVariantSchema::Entry declaration;
                    declaration.type     = key->optionType;
                    declaration.bitWidth = key->optionWidth;
                    ShaderVariantEntry value;
                    value.type = key->optionType;
                    if (const auto *typed = std::get_if<bool>(&after.value))
                        value.value = *typed ? 1u : 0u;
                    else if (const auto *typed = std::get_if<int32_t>(&after.value))
                        value.value = static_cast<uint32_t>(*typed);
                    else if (const auto *typed = std::get_if<uint32_t>(&after.value))
                        value.value = *typed;
                    if (!ValidateShaderVariantValue(declaration, value))
                        return sky::editor::UndoCommandPtr{};
                }
                auto apply = [this, index](const MaterialPropertyValue &value, bool overridden) {
                    rows[index].value       = value;
                    rows[index].overridden  = overridden;
                    formValues[index].value = value;
                    if (isInstance && !overridden) {
                        std::erase_if(instanceSource.properties, [&](const auto &property) { return property.name == rows[index].name; });
                        dirty = true;
                    } else {
                        PersistProperty(static_cast<int>(index));
                    }
                };
                return sky::editor::UndoCommandPtr(
                    std::make_unique<MaterialValueEditCommand>(generation, before, after.value, rows[index].overridden, std::move(apply)));
            });
            form->SetOnEdited([this, generation]() {
                if (!generation.expired()) {
                    dirty = true;
                    MarkPaintDirty();
                }
            });
            propertyForms.push_back(static_cast<sky::editor::ReflectedFormView *>(AddChild(std::move(form))));
        }
        MarkLayoutDirty();
    }

    bool MaterialEditorView::IsEditing() const
    {
        return editingRow >= 0 || pickerRow >= 0 || std::ranges::any_of(propertyForms, [](const auto *form) { return form->HasActiveInteraction(); });
    }

    void MaterialEditorView::CancelInteractions()
    {
        editingRow = pickerRow = -1;
        for (auto *form : propertyForms)
            form->CancelInteractions();
    }

    void MaterialEditorView::Tick()
    {
        if (reloadPending && !IsEditing()) {
            reloadPending = false;
            ReloadTechniques();
        }
    }

    void MaterialEditorView::AddTechnique()
    {
        for (size_t index = 0; index < rows.size(); ++index)
            if (rows[index].isString)
                PersistProperty(static_cast<int>(index));
        materialSource.techniques.emplace_back();
        reloadPending = dirty = true;
        MarkLayoutDirty();
    }

    void MaterialEditorView::DeleteTechnique(int techniqueIndex)
    {
        if (techniqueIndex < 0 || techniqueIndex >= static_cast<int>(materialSource.techniques.size()))
            return;
        for (size_t index = 0; index < rows.size(); ++index)
            if (rows[index].isString)
                PersistProperty(static_cast<int>(index));
        materialSource.techniques.erase(materialSource.techniques.begin() + techniqueIndex);
        reloadPending = dirty = true;
        MarkLayoutDirty();
    }

    bool MaterialEditorView::Save()
    {
        if (!dirty) {
            return false;
        }
        for (size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].kind != MaterialRowKind::Field || (isInstance && !rows[i].overridden)) {
                continue;
            }
            if (!PersistProperty(static_cast<int>(i))) {
                return false;
            }
        }
        ReloadTechniques();
        if (!schemaError.empty() || !(isInstance ? SaveSourceJson(asset, instanceSource) : SaveSourceJson(asset, materialSource)))
            return false;
        sky::editor::AssetCookService::Get()->RequestCook(asset);
        dirty = false;
        MarkPaintDirty();
        return true;
    }

    void MaterialEditorView::Measure(float &outWidth, float &outHeight)
    {
        outWidth      = 0.0f;
        const auto &m = Theme().metrics;
        outHeight =
            m.panelHeaderHeight +
            static_cast<float>(std::ranges::count_if(rows, [](const auto &row) { return row.isString || row.kind != MaterialRowKind::Field; })) *
                m.rowHeight +
            2.0f * m.controlPad;
        for (auto *form : propertyForms) {
            form->SynchronizeLayout();
            outHeight += form->GetPreferredHeight() + m.controlPad;
        }
    }

    void MaterialEditorView::RebuildRowSlots()
    {
        if (rowsPanel == nullptr) {
            return;
        }
        // A uniform grid follows the current theme's row height on every arrangement.
        rowsPanel->ClearChildren();
        rowsPanel->SetGrid(
            static_cast<int>(std::ranges::count_if(rows, [](const auto &row) { return row.isString || row.kind != MaterialRowKind::Field; })), 1);
        for (size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].isString || rows[i].kind != MaterialRowKind::Field)
                rowsPanel->Add(std::make_unique<sky::ui::UIElement>(), {static_cast<int>(rowsPanel->GetChildren().size()), 0});
        }
        MarkLayoutDirty();
    }

    void MaterialEditorView::ArrangeChildren(const sky::ui::UIRect & /*content*/)
    {
        if (rowsPanel == nullptr) {
            return;
        }
        const auto  content = skin.PanelContentRect(GetBounds());
        const auto &m       = Theme().metrics;
        const float top     = content.top + m.controlPad;
        float       bottom  = top + static_cast<float>(
                                 std::ranges::count_if(rows, [](const auto &row) { return row.isString || row.kind != MaterialRowKind::Field; })) *
                                 m.rowHeight;
        rowsPanel->Arrange({content.left, top, content.right, bottom});
        for (auto *form : propertyForms) {
            form->SynchronizeLayout();
            const float height = form->GetPreferredHeight();
            bottom += m.controlPad;
            form->Arrange({content.left, bottom, content.right, bottom + height});
            bottom += height;
        }
    }

    float MaterialEditorView::LabelWidth() const
    {
        return std::min(Theme().metrics.fieldWidth, std::max(0.0f, GetBounds().Width() * 0.4f));
    }

    sky::ui::UIRect MaterialEditorView::PickerNoneRect() const
    {
        const auto &m = Theme().metrics;
        return {pickerRect.left + m.cellPadding, pickerRect.top + m.cellPadding, pickerRect.right - m.cellPadding,
                pickerRect.top + m.cellPadding + m.popupItemHeight};
    }

    sky::ui::UIRect MaterialEditorView::RowRect(size_t index) const
    {
        if (!rowsPanel || index >= rows.size() || (!rows[index].isString && rows[index].kind == MaterialRowKind::Field))
            return {};
        size_t slot = 0;
        for (size_t i = 0; i < index; ++i)
            if (rows[i].isString || rows[i].kind != MaterialRowKind::Field)
                ++slot;
        return slot < rowsPanel->GetChildren().size() ? rowsPanel->GetChildren()[slot]->GetBounds() : sky::ui::UIRect{};
    }

    void MaterialEditorView::OnPaint(sky::ui::UIPaintContext &context)
    {
        const auto           &th     = Theme();
        const auto           &m      = th.metrics;
        const float           pixel  = m.checkboxPad * 0.5f;
        const sky::ui::UIRect bounds = GetBounds();
        skin.DrawPanel(context, bounds, title);

        for (size_t i = 0; i < rows.size(); ++i) {
            const MaterialEditorRow &prop = rows[i];
            if (!prop.isString && prop.kind == MaterialRowKind::Field)
                continue;
            const sky::ui::UIRect row = RowRect(i);
            if (row.bottom < bounds.top || row.top > bounds.bottom) {
                continue;
            }
            skin.DrawRow(context, row, i % 2 ? sky::editor::RowState::Alt : sky::editor::RowState::Normal);

            if (prop.kind == MaterialRowKind::Diagnostic) {
                uc::Text(context, prop.name, th.fonts.label, row, th.colors.textDisabled, text);
                continue;
            }
            if (prop.kind == MaterialRowKind::TechniqueHeader) {
                skin.DrawSectionHeader(context, row, prop.name);
                const sky::ui::UIRect del{row.right - m.controlPad - m.frameHeight, row.top + pixel, row.right - m.controlPad, row.bottom - pixel};
                skin.DrawToolItem(context, del, "x", hoverRow == static_cast<int>(i));
                continue;
            }
            if (prop.kind == MaterialRowKind::AddTechnique) {
                skin.DrawToolItem(context, row, prop.name, hoverRow == static_cast<int>(i));
                continue;
            }

            uc::Text(context, prop.name, th.fonts.label, {row.left + m.controlPad, row.top, row.left + m.controlPad + LabelWidth(), row.bottom},
                     th.colors.text, text);

            const bool            isTexture = IsTextureProperty(prop.type);
            const float           boxRight  = isTexture ? row.right - m.controlPad - m.frameHeight - m.checkboxPad : row.right - m.controlPad;
            const sky::ui::UIRect box{row.left + m.controlPad + LabelWidth(), row.top + pixel, boxRight, row.bottom - pixel};
            const bool            editing = editingRow == static_cast<int>(i);
            skin.DrawField(context, box, editing, false);

            if (isTexture) {
                const Uuid *texturePtr = std::get_if<Uuid>(&prop.value);
                const bool  hasTexture = texturePtr != nullptr && static_cast<bool>(*texturePtr);
                uc::Text(context, DisplayText(prop), th.fonts.value, {box.left + m.cellPadding, box.top, box.right - m.cellPadding, box.bottom},
                         hasTexture ? th.colors.text : th.colors.textDisabled, text);
                const sky::ui::UIRect browse{row.right - m.controlPad - m.frameHeight, row.top + pixel, row.right - m.controlPad, row.bottom - pixel};
                skin.DrawToolItem(context, browse, "...", hoverRow == static_cast<int>(i));
            } else {
                const std::string shown = editing ? editText : DisplayText(prop);
                const uint32_t    color =
                    prop.type == MaterialPropertyType::MAT4 || prop.type == MaterialPropertyType::SAMPLER ? th.colors.textDisabled : th.colors.text;
                uc::Text(context, shown, th.fonts.value, {box.left + m.cellPadding, box.top, box.right - m.cellPadding, box.bottom}, color, text);
                if (editing && text != nullptr) {
                    const float caretX =
                        box.left + m.cellPadding + sky::ui::UITextLayout::Measure(shown, th.fonts.value, text->GetAtlas()).width + pixel;
                    uc::Fill(context, {caretX, box.top + m.checkboxPad, caretX + pixel, box.bottom - m.checkboxPad}, th.colors.text);
                }
            }
        }

        if (pickerRow >= 0) {
            skin.DrawPopup(context, pickerRect);
            const auto noneRect = PickerNoneRect();
            skin.DrawPopupItem(context, noneRect, false, true);
            uc::Text(context, "(none)", th.fonts.label,
                     {noneRect.left + m.cellPadding, noneRect.top, noneRect.right - m.cellPadding, noneRect.bottom}, th.colors.textMuted, text);
            for (size_t i = 0; i < pickerRects.size(); ++i) {
                skin.DrawPopupItem(context, pickerRects[i], static_cast<int>(i) == pickerHover, false);
                const auto        &rect  = pickerRects[i];
                const std::string &label = pickerItems[i].name.empty() ? pickerItems[i].uuid.ToString() : pickerItems[i].name;
                uc::Text(context, label, th.fonts.label, {rect.left + m.cellPadding, rect.top, rect.right - m.cellPadding, rect.bottom},
                         th.colors.text, text);
            }
        }
    }

    bool MaterialEditorView::WantsPointerCapture() const
    {
        return pickerRow >= 0 || editingRow >= 0;
    }

    void MaterialEditorView::CommitValue(int index)
    {
        if (index < 0 || index >= static_cast<int>(rows.size())) {
            return;
        }
        MaterialEditorRow &p = rows[index];
        if (p.isString) {
            p.text = editText;
            dirty  = true;
            return;
        }
        if (p.type == MaterialPropertyType::FLOAT) {
            p.value = std::strtof(editText.c_str(), nullptr);
        } else if (p.type == MaterialPropertyType::INT) {
            p.value = static_cast<int32_t>(std::strtol(editText.c_str(), nullptr, 10));
        } else if (p.type == MaterialPropertyType::UINT) {
            p.value = static_cast<uint32_t>(std::strtoul(editText.c_str(), nullptr, 10));
        } else if (p.type == MaterialPropertyType::BOOL) {
            p.value = (editText == "true" || editText == "1");
        } else if (p.type == MaterialPropertyType::FLOAT2 || p.type == MaterialPropertyType::FLOAT3 || p.type == MaterialPropertyType::FLOAT4) {
            std::vector<float> floats;
            const size_t       n = SizeOfPropertyType(p.type) / sizeof(float);
            if (!ParseFloats(editText, floats, n)) {
                return;
            }
            if (n == 2) {
                Vector2 v{};
                v.v[0]  = floats[0];
                v.v[1]  = floats[1];
                p.value = v;
            } else if (n == 3) {
                Vector3 v{};
                v.v[0]  = floats[0];
                v.v[1]  = floats[1];
                v.v[2]  = floats[2];
                p.value = v;
            } else {
                Vector4 v{};
                v.v[0]  = floats[0];
                v.v[1]  = floats[1];
                v.v[2]  = floats[2];
                v.v[3]  = floats[3];
                p.value = v;
            }
        } else {
            return;
        }
        PersistProperty(index);
    }

    void MaterialEditorView::CommitTexture(int index, const Uuid &id)
    {
        if (index < 0 || index >= static_cast<int>(rows.size())) {
            return;
        }
        rows[index].value = id;
        PersistProperty(index);
    }

    bool MaterialEditorView::PersistProperty(int index)
    {
        if (index < 0 || index >= static_cast<int>(rows.size())) {
            return false;
        }
        const MaterialEditorRow &prop = rows[index];

        if (prop.isString) {
            if (isInstance || prop.techniqueIndex < 0 || prop.techniqueIndex >= static_cast<int>(materialSource.techniques.size())) {
                return false;
            }
            MaterialTechniqueData &tech = materialSource.techniques[prop.techniqueIndex];
            if (prop.techniqueField == 10) {
                SetRenderStateValue(tech.renderState, prop.renderKey, prop.text);
            } else if (prop.techniqueField == 0) {
                tech.tag = prop.text;
            } else if (prop.techniqueField == 1) {
                tech.program.vertex.relativePath = prop.text;
            } else if (prop.techniqueField == 2) {
                tech.program.vertex.entry = prop.text;
            } else if (prop.techniqueField == 3) {
                tech.program.fragment.relativePath = prop.text;
            } else if (prop.techniqueField == 4) {
                tech.program.fragment.entry = prop.text;
            }
        } else {
            if (!MaterialPropertyValueMatches(prop.type, prop.value)) {
                LOG_W(TAG, "cannot persist a material property with a mismatched semantic value type");
                return false;
            }
            std::vector<MaterialPropertyData> &props = isInstance ? instanceSource.properties : materialSource.properties;
            const auto                         found = std::ranges::find(props, prop.name, &MaterialPropertyData::name);
            if (found == props.end())
                props.push_back({prop.name, prop.type, prop.value});
            else
                *found = {prop.name, prop.type, prop.value};
        }
        dirty = true;
        MarkPaintDirty();
        return true;
    }

    void MaterialEditorView::OpenPicker(int index, const sky::ui::UIRect &row)
    {
        pickerRow   = index;
        pickerHover = -1;
        pickerItems.clear();
        if (auto *catalog = sky::editor::GetEditorAssetCatalog()) {
            for (const auto &item : catalog->Gather(std::string(AssetTraits<Texture>::ASSET_TYPE))) {
                pickerItems.push_back(PickerItem{item.uuid, item.name});
            }
        }
        const auto &m    = Theme().metrics;
        const float top  = row.bottom + m.checkboxPad;
        const float h    = 2.0f * m.cellPadding + static_cast<float>(pickerItems.size() + 1) * m.popupItemHeight;
        const float left = row.left + m.controlPad + LabelWidth();
        pickerRect       = {left, top, left + m.popupMinWidth, top + h};
        pickerRects.clear();
        for (size_t k = 0; k < pickerItems.size(); ++k) {
            const float iy = pickerRect.top + m.cellPadding + static_cast<float>(k + 1) * m.popupItemHeight;
            pickerRects.push_back({pickerRect.left + m.cellPadding, iy, pickerRect.right - m.cellPadding, iy + m.popupItemHeight});
        }
        MarkPaintDirty();
    }

    sky::ui::UIEventResult MaterialEditorView::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (reloadPending)
            return sky::ui::UIEventResult::HANDLED;
        if (event.action == sky::ui::UIPointerAction::WHEEL) {
            return sky::ui::UIEventResult::UNHANDLED; // let the viewer scroll the whole region
        }

        if (pickerRow >= 0) {
            if (event.action == sky::ui::UIPointerAction::MOVE) {
                pickerHover = -1;
                for (size_t i = 0; i < pickerRects.size(); ++i) {
                    if (pickerRects[i].Contains(event.x, event.y)) {
                        pickerHover = static_cast<int>(i);
                    }
                }
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (event.action == sky::ui::UIPointerAction::DOWN) {
                const sky::ui::UIRect noneRect = PickerNoneRect();
                if (noneRect.Contains(event.x, event.y)) {
                    const int row = pickerRow;
                    pickerRow     = -1;
                    CommitTexture(row, Uuid{});
                    return sky::ui::UIEventResult::HANDLED;
                }
                for (size_t i = 0; i < pickerRects.size(); ++i) {
                    if (pickerRects[i].Contains(event.x, event.y)) {
                        const int  row = pickerRow;
                        const Uuid id  = pickerItems[i].uuid;
                        pickerRow      = -1;
                        CommitTexture(row, id);
                        return sky::ui::UIEventResult::HANDLED;
                    }
                }
                pickerRow = -1;
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::MOVE) {
            int hovered = -1;
            for (size_t i = 0; i < rows.size(); ++i) {
                if (RowRect(i).Contains(event.x, event.y)) {
                    hovered = static_cast<int>(i);
                    break;
                }
            }
            if (hovered != hoverRow) {
                hoverRow = hovered;
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::HANDLED;
        }

        if (editingRow >= 0) {
            CommitValue(editingRow);
            editingRow = -1;
        }
        for (size_t i = 0; i < rows.size(); ++i) {
            const sky::ui::UIRect row = RowRect(i);
            if (rows[i].kind == MaterialRowKind::Diagnostic || (!rows[i].isString && rows[i].kind == MaterialRowKind::Field) ||
                !row.Contains(event.x, event.y)) {
                continue;
            }
            if (rows[i].kind == MaterialRowKind::TechniqueHeader) {
                const sky::ui::UIRect del{row.right - Theme().metrics.controlPad - Theme().metrics.frameHeight,
                                          row.top + Theme().metrics.checkboxPad * 0.5f, row.right - Theme().metrics.controlPad,
                                          row.bottom - Theme().metrics.checkboxPad * 0.5f};
                if (del.Contains(event.x, event.y)) {
                    DeleteTechnique(rows[i].techniqueIndex);
                }
                return sky::ui::UIEventResult::HANDLED;
            }
            if (rows[i].kind == MaterialRowKind::AddTechnique) {
                AddTechnique();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (IsTextureProperty(rows[i].type)) {
                const sky::ui::UIRect browse{row.right - Theme().metrics.controlPad - Theme().metrics.frameHeight, row.top,
                                             row.right - Theme().metrics.controlPad, row.bottom};
                if (browse.Contains(event.x, event.y)) {
                    OpenPicker(static_cast<int>(i), row);
                }
            } else if (rows[i].type != MaterialPropertyType::MAT4 && rows[i].type != MaterialPropertyType::SAMPLER) {
                editingRow = static_cast<int>(i);
                editText   = EditableText(rows[i]);
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult MaterialEditorView::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        if (editingRow < 0 || event.action == sky::ui::UIKeyAction::UP) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        constexpr uint32_t kEnter  = 0x0D;
        constexpr uint32_t kEscape = 0x1B;
        constexpr uint32_t kBack   = 0x08;
        if (event.keyCode == kEnter) {
            CommitValue(editingRow);
            editingRow = -1;
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.keyCode == kEscape) {
            editingRow = -1;
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.keyCode == kBack) {
            if (!editText.empty()) {
                editText.pop_back();
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    sky::ui::UIEventResult MaterialEditorView::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        if (editingRow < 0 || event.text.empty()) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        editText += event.text;
        MarkPaintDirty();
        return sky::ui::UIEventResult::HANDLED;
    }

    namespace {

        class MaterialDocument final : public sky::editor::IAssetViewDocument {
        public:
            MaterialDocument(Uuid asset, bool instance) : asset(asset), instance(instance)
            {
            }
            std::unique_ptr<sky::ui::UIElement> CreateContent(sky::ui::UITextSystem *text) override
            {
                auto content =
                    std::make_unique<MaterialEditorView>(text, asset, instance ? "Material Instance" : "Material", instance, !instance, &history);
                view = content->GetInputReference();
                return content;
            }
            sky::editor::CommandService &History() override
            {
                return history;
            }
            bool IsDirty() const override
            {
                const auto *v = View();
                return v && v->HasPendingEdits();
            }
            bool IsEditing() const override
            {
                const auto *v = View();
                return v && v->IsEditing();
            }
            void CancelInteractions() override
            {
                if (auto *v = View())
                    v->CancelInteractions();
            }
            void RequestSave() override
            {
                savePending = true;
            }
            bool IsSavePending() const override
            {
                return savePending;
            }
            std::string Status() const override
            {
                const auto *v = View();
                if (v && !v->Error().empty())
                    return v->Error();
                return status;
            }
            void Tick(float) override
            {
                auto *v = View();
                if (!v)
                    return;
                v->Tick();
                if (savePending && !v->IsEditing()) {
                    savePending = false;
                    status      = !v->HasPendingEdits() || v->Save() ? "Saved" : "Material validation or source save failed";
                }
            }

        private:
            MaterialEditorView *View() const
            {
                return static_cast<MaterialEditorView *>(view.GetAlive());
            }
            Uuid                               asset;
            bool                               instance;
            bool                               savePending = false;
            sky::editor::CommandService        history;
            sky::ui::UIElement::InputReference view;
            std::string                        status;
        };

        class MaterialViewProvider final : public sky::editor::IAssetViewProvider {
        public:
            explicit MaterialViewProvider(bool instance) : instance(instance)
            {
            }
            bool UsesDocumentWindow() const override
            {
                return true;
            }
            sky::editor::AssetViewDocument OpenDocument(const Uuid &asset, const std::string &type) override
            {
                auto lease = sky::editor::AssetViewProviderRegistry::Get()->Retain(type);
                if (!lease)
                    return {};
                return {std::move(lease), std::make_unique<MaterialDocument>(asset, instance)};
            }

        private:
            bool instance;
        };

        MaterialViewProvider g_materialView(false);
        MaterialViewProvider g_materialInstanceView(true);

    } // namespace

    void RegisterMaterialEditorProvider(std::shared_ptr<const void> lease)
    {
        auto *registry = sky::editor::AssetViewProviderRegistry::Get();
        registry->Register(std::string(AssetTraits<Material>::ASSET_TYPE), &g_materialView, lease);
        registry->Register(std::string(AssetTraits<MaterialInstance>::ASSET_TYPE), &g_materialInstanceView, std::move(lease));
    }
    void UnregisterMaterialEditorProvider()
    {
        auto *registry = sky::editor::AssetViewProviderRegistry::Get();
        registry->Unregister(std::string(AssetTraits<Material>::ASSET_TYPE), &g_materialView);
        registry->Unregister(std::string(AssetTraits<MaterialInstance>::ASSET_TYPE), &g_materialInstanceView);
    }

} // namespace sky::aurora::editor
