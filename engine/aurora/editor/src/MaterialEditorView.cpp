//
// Material / material-instance property editor (custom asset-view content). See MaterialEditorView.h.
//

#include <aurora/editor/MaterialEditorView.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/adaptor/assets/MaterialReflection.h>
#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/JsonArchive.h>

#include <core/logger/Logger.h>
#include <editor/core/asset/AssetCookService.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/ui/AssetViewProvider.h>
#include <editor/ui/UiDraw.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>

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
                    } else {
                        std::string           s;
                        constexpr std::size_t n = sizeof(v.v) / sizeof(float);
                        for (std::size_t i = 0; i < n; ++i) {
                            if (i != 0) {
                                s += ", ";
                            }
                            s += FloatToString(v.v[i]);
                        }
                        return s;
                    }
                },
                r.value);
        }

        std::string DisplayText(const MaterialEditorRow &r)
        {
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
            return true;
        }

    } // namespace

    MaterialEditorView::MaterialEditorView(
        sky::ui::UITextSystem *textSystem, const Uuid &uuid, std::string titleText, bool instance, bool enableTechniques)
        : text(textSystem)
        , skin(sky::editor::GetDefaultUiTheme(), textSystem)
        , asset(uuid)
        , title(std::move(titleText))
        , isInstance(instance)
        , allowTechniques(enableTechniques)
    {
        SetFocusable(true);
        rowsPanel = static_cast<sky::ui::GridPanel *>(AddChild(std::make_unique<sky::ui::GridPanel>()));
        if (isInstance) {
            LoadSourceJson(asset, instanceSource);
        } else {
            LoadSourceJson(asset, materialSource);
        }
        ReloadTechniques();
    }

    MaterialEditorView::~MaterialEditorView() = default;

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
        rows.clear();

        auto appendProperties = [&](const std::vector<MaterialPropertyData> &props) {
            for (const auto &p : props) {
                rows.push_back(MaterialEditorRow{p.name, p.type, p.value});
            }
        };

        if (isInstance) {
            appendProperties(instanceSource.properties);
        } else {
            appendProperties(materialSource.properties);
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
                    AddRow(rows, "shader", t.shader.relativePath, ti, 1);
                    AddRow(rows, "entry", t.shader.entry, ti, 2);
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
        MarkPaintDirty();
    }

    void MaterialEditorView::AddTechnique()
    {
        MaterialTechniqueData technique;
        technique.shader.entry = "main";
        materialSource.techniques.push_back(std::move(technique));
        SaveSourceJson(asset, materialSource);
        ReloadTechniques();
        dirty = true;
    }

    void MaterialEditorView::DeleteTechnique(int techniqueIndex)
    {
        if (techniqueIndex < 0 || techniqueIndex >= static_cast<int>(materialSource.techniques.size())) {
            return;
        }
        materialSource.techniques.erase(materialSource.techniques.begin() + techniqueIndex);
        SaveSourceJson(asset, materialSource);
        ReloadTechniques();
        dirty = true;
    }

    bool MaterialEditorView::Save()
    {
        if (!dirty) {
            return false;
        }
        for (size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].kind != MaterialRowKind::Field) {
                continue;
            }
            PersistProperty(static_cast<int>(i));
        }
        sky::editor::AssetCookService::Get()->RequestCook(asset);
        dirty = false;
        MarkPaintDirty();
        return true;
    }

    void MaterialEditorView::Measure(float &outWidth, float &outHeight)
    {
        outWidth      = 0.0f;
        const auto &m = Theme().metrics;
        outHeight     = m.panelHeaderHeight + static_cast<float>(rows.size()) * m.rowHeight + 2.0f * m.controlPad;
    }

    void MaterialEditorView::RebuildRowSlots()
    {
        if (rowsPanel == nullptr) {
            return;
        }
        // A uniform grid follows the current theme's row height on every arrangement.
        rowsPanel->ClearChildren();
        rowsPanel->SetGrid(static_cast<int>(rows.size()), 1);
        for (size_t i = 0; i < rows.size(); ++i) {
            rowsPanel->Add(std::make_unique<sky::ui::UIElement>(), {static_cast<int>(i), 0});
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
        rowsPanel->Arrange({content.left, top, content.right, top + static_cast<float>(rows.size()) * m.rowHeight});
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
        if (rowsPanel == nullptr || index >= rowsPanel->GetChildren().size()) {
            return sky::ui::UIRect{};
        }
        return rowsPanel->GetChildren()[index]->GetBounds();
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
            const sky::ui::UIRect    row  = RowRect(i);
            if (row.bottom < bounds.top || row.top > bounds.bottom) {
                continue;
            }
            skin.DrawRow(context, row, i % 2 ? sky::editor::RowState::Alt : sky::editor::RowState::Normal);

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
                const uint32_t    color = prop.type == MaterialPropertyType::MAT4 ? th.colors.textDisabled : th.colors.text;
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
                tech.shader.relativePath = prop.text;
            } else if (prop.techniqueField == 2) {
                tech.shader.entry = prop.text;
            }
        } else {
            std::vector<MaterialPropertyData> &props = isInstance ? instanceSource.properties : materialSource.properties;
            for (auto &p : props) {
                if (p.name == prop.name) {
                    p.type  = prop.type;
                    p.value = prop.value;
                    break;
                }
            }
        }

        const bool ok = isInstance ? SaveSourceJson(asset, instanceSource) : SaveSourceJson(asset, materialSource);
        if (!ok) {
            LOG_W(TAG, "cannot write source for %s", asset.ToString().c_str());
        }
        MarkPaintDirty();
        return ok;
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
            if (!row.Contains(event.x, event.y)) {
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
            } else if (rows[i].type != MaterialPropertyType::MAT4) {
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

        class MaterialViewProvider : public sky::editor::IAssetViewProvider {
        public:
            bool SaveContent(const Uuid &, const std::string &, sky::ui::UIElement *content) override
            {
                auto *view = static_cast<MaterialEditorView *>(content);
                return view != nullptr && view->Save();
            }

            std::unique_ptr<sky::ui::UIElement> CreateContent(const Uuid &uuid, const std::string &, sky::ui::UITextSystem *text) override
            {
                // The view loads the reflected `.mat` source itself (no parser).
                return std::make_unique<MaterialEditorView>(text, uuid, "Material", /*isInstance=*/false, /*enableTechniques=*/true);
            }
        };

        class MaterialInstanceViewProvider : public sky::editor::IAssetViewProvider {
        public:
            bool SaveContent(const Uuid &, const std::string &, sky::ui::UIElement *content) override
            {
                auto *view = static_cast<MaterialEditorView *>(content);
                return view != nullptr && view->Save();
            }

            std::unique_ptr<sky::ui::UIElement> CreateContent(const Uuid &uuid, const std::string &, sky::ui::UITextSystem *text) override
            {
                return std::make_unique<MaterialEditorView>(text, uuid, "Material Instance", /*isInstance=*/true, /*enableTechniques=*/false);
            }
        };

        MaterialViewProvider         g_materialView;
        MaterialInstanceViewProvider g_materialInstanceView;

    } // namespace

    void RegisterMaterialEditorProvider()
    {
        auto *registry = sky::editor::AssetViewProviderRegistry::Get();
        registry->Register(std::string(AssetTraits<Material>::ASSET_TYPE), &g_materialView);
        registry->Register(std::string(AssetTraits<MaterialInstance>::ASSET_TYPE), &g_materialInstanceView);
    }

} // namespace sky::aurora::editor
