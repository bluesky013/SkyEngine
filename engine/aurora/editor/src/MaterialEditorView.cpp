//
// Material / material-instance property editor (custom asset-view content). See MaterialEditorView.h.
//

#include <aurora/editor/MaterialEditorView.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>

#include <core/logger/Logger.h>
#include <editor/core/asset/AssetCookService.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/shell/AssetViewProvider.h>
#include <editor/shell/UiDraw.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/StackPanel.h>

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

        constexpr float    kRowH    = 22.0f;
        constexpr float    kPad     = 8.0f;
        constexpr float    kTitleH  = 22.0f;
        constexpr float    kLabelW  = 190.0f;
        constexpr float    kBrowseW = 26.0f;
        constexpr uint32_t kTextSz  = 12;

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
            switch (r.type) {
            case MaterialPropertyType::FLOAT: return FloatToString(ReadPod<float>(r.value));
            case MaterialPropertyType::INT: return std::to_string(ReadPod<int32_t>(r.value));
            case MaterialPropertyType::UINT: return std::to_string(ReadPod<uint32_t>(r.value));
            case MaterialPropertyType::BOOL: return ReadPod<uint8_t>(r.value) ? "true" : "false";
            case MaterialPropertyType::FLOAT2:
            case MaterialPropertyType::FLOAT3:
            case MaterialPropertyType::FLOAT4: {
                const int   n = static_cast<int>(SizeOfPropertyType(r.type) / sizeof(float));
                std::string s;
                for (int i = 0; i < n; ++i) {
                    if (i != 0) {
                        s += ", ";
                    }
                    s += FloatToString(ReadPod<float>(r.value, i * sizeof(float)));
                }
                return s;
            }
            case MaterialPropertyType::MAT4: return "(matrix)";
            default: return {};
            }
        }

        std::string DisplayText(const MaterialEditorRow &r)
        {
            if (IsTextureProperty(r.type)) {
                if (!r.texture) {
                    return "(none)";
                }
                std::string name;
                if (auto *catalog = sky::editor::GetEditorAssetCatalog(); catalog != nullptr && catalog->GetName(r.texture, name)) {
                    return name;
                }
                return r.texture.ToString();
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

    } // namespace

    MaterialEditorView::MaterialEditorView(
        sky::ui::UITextSystem *textSystem, const Uuid &uuid, std::string titleText, std::vector<MaterialEditorRow> propRows, bool enableTechniques)
        : text(textSystem), asset(uuid), title(std::move(titleText)), propertyRows(std::move(propRows)), allowTechniques(enableTechniques)
    {
        SetFocusable(true);
        rowsPanel = static_cast<sky::ui::StackPanel *>(AddChild(std::make_unique<sky::ui::StackPanel>(sky::ui::StackOrientation::Vertical)));
        rowsPanel->SetGap(0.0f);
        rowsPanel->SetPadding(0.0f);
        ReloadTechniques();
    }

    MaterialEditorView::~MaterialEditorView() = default;

    namespace {

        bool ReadSourceJson(const Uuid &asset, rapidjson::Document &doc, std::string &abs)
        {
            abs = SourceAbsolutePath(asset);
            if (abs.empty()) {
                return false;
            }
            std::ifstream in(abs, std::ios::binary);
            if (!in) {
                return false;
            }
            std::stringstream ss;
            ss << in.rdbuf();
            doc.Parse(ss.str().c_str());
            return !doc.HasParseError() && doc.IsObject();
        }

        bool WriteSourceJson(const std::string &abs, const rapidjson::Document &doc)
        {
            rapidjson::StringBuffer                    sb;
            rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
            doc.Accept(writer);
            std::ofstream out(abs, std::ios::binary | std::ios::trunc);
            if (!out) {
                return false;
            }
            out.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
            return true;
        }

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

    } // namespace

    void MaterialEditorView::ReloadTechniques()
    {
        rows.clear();
        for (const auto &p : propertyRows) {
            rows.push_back(p);
        }
        rapidjson::Document doc;
        std::string         abs;
        if (allowTechniques && ReadSourceJson(asset, doc, abs) && doc.HasMember("techniques") && doc["techniques"].IsArray()) {
            int techIndex = 0;
            for (const auto &t : doc["techniques"].GetArray()) {
                if (!t.IsObject()) {
                    continue;
                }
                const std::string tag = (t.HasMember("tag") && t["tag"].IsString()) ? t["tag"].GetString() : std::to_string(techIndex);

                MaterialEditorRow header;
                header.kind           = MaterialRowKind::TechniqueHeader;
                header.techniqueIndex = techIndex;
                header.name           = "Technique " + std::to_string(techIndex) + (tag.empty() ? "" : " [" + tag + "]");
                rows.push_back(std::move(header));

                AddRow(rows, "tag", (t.HasMember("tag") && t["tag"].IsString()) ? t["tag"].GetString() : "", techIndex, 0);
                AddRow(rows, "shader", (t.HasMember("shader") && t["shader"].IsString()) ? t["shader"].GetString() : "", techIndex, 1);
                AddRow(rows, "entry", (t.HasMember("entry") && t["entry"].IsString()) ? t["entry"].GetString() : "main", techIndex, 2);

                if (t.HasMember("renderState") && t["renderState"].IsObject()) {
                    const auto &rs      = t["renderState"];
                    auto        addBool = [&](const char *key) {
                        if (rs.HasMember(key)) {
                            AddRow(rows, std::string("rs.") + key, rs[key].GetBool() ? "true" : "false", techIndex, 10, key);
                        }
                    };
                    auto addStr = [&](const char *key) {
                        if (rs.HasMember(key) && rs[key].IsString()) {
                            AddRow(rows, std::string("rs.") + key, rs[key].GetString(), techIndex, 10, key);
                        }
                    };
                    addBool("depthTest");
                    addBool("depthWrite");
                    addBool("stencilTest");
                    addStr("depthCompare");
                    addStr("cullMode");
                    addStr("frontFace");
                    addStr("polygonMode");
                    addBool("alphaToCoverage");
                    if (rs.HasMember("depthBias") && rs["depthBias"].IsObject()) {
                        const auto &b = rs["depthBias"];
                        if (b.HasMember("constant")) {
                            AddRow(rows, "rs.depthBias.constant", FloatToString(b["constant"].GetFloat()), techIndex, 10, "depthBias.constant");
                        }
                        if (b.HasMember("slope")) {
                            AddRow(rows, "rs.depthBias.slope", FloatToString(b["slope"].GetFloat()), techIndex, 10, "depthBias.slope");
                        }
                        if (b.HasMember("clamp")) {
                            AddRow(rows, "rs.depthBias.clamp", FloatToString(b["clamp"].GetFloat()), techIndex, 10, "depthBias.clamp");
                        }
                    }
                }
                ++techIndex;
            }
        }

        if (allowTechniques) {
            MaterialEditorRow add;
            add.kind = MaterialRowKind::AddTechnique;
            add.name = "+ Add Technique";
            rows.push_back(std::move(add));
        }

        RebuildRowSlots();
        MarkPaintDirty();
    }

    void MaterialEditorView::AddTechnique()
    {
        rapidjson::Document doc;
        std::string         abs;
        if (!ReadSourceJson(asset, doc, abs)) {
            return;
        }
        auto &alloc = doc.GetAllocator();
        if (!doc.HasMember("techniques") || !doc["techniques"].IsArray()) {
            doc.RemoveMember("techniques");
            doc.AddMember("techniques", rapidjson::Value(rapidjson::kArrayType), alloc);
        }
        rapidjson::Value entry(rapidjson::kObjectType);
        entry.AddMember(rapidjson::Value("tag", alloc), rapidjson::Value("", alloc), alloc);
        entry.AddMember(rapidjson::Value("shader", alloc), rapidjson::Value("", alloc), alloc);
        entry.AddMember(rapidjson::Value("entry", alloc), rapidjson::Value("main", alloc), alloc);
        doc["techniques"].PushBack(entry, alloc);
        if (!WriteSourceJson(abs, doc)) {
            return;
        }
        ReloadTechniques();
        dirty = true;
    }

    void MaterialEditorView::DeleteTechnique(int techniqueIndex)
    {
        rapidjson::Document doc;
        std::string         abs;
        if (!ReadSourceJson(asset, doc, abs) || !doc.HasMember("techniques") || !doc["techniques"].IsArray()) {
            return;
        }
        rapidjson::Value &techs = doc["techniques"];
        if (techniqueIndex < 0 || techniqueIndex >= static_cast<int>(techs.Size())) {
            return;
        }
        techs.Erase(techs.Begin() + techniqueIndex);
        if (!WriteSourceJson(abs, doc)) {
            return;
        }
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
        sky::editor::AssetCookService::Get()->TriggerCook(asset);
        dirty = false;
        MarkPaintDirty();
        return true;
    }

    void MaterialEditorView::Measure(float &outWidth, float &outHeight)
    {
        (void)outWidth;
        outHeight = kPad + kTitleH + static_cast<float>(rows.size()) * kRowH + kPad;
    }

    void MaterialEditorView::RebuildRowSlots()
    {
        if (rowsPanel == nullptr) {
            return;
        }
        // Each row is a fixed-height child of the vertical stack; the panel owns the geometry
        // (the view paints the rows from the arranged child bounds in OnPaint).
        rowsPanel->ClearChildren();
        for (size_t i = 0; i < rows.size(); ++i) {
            sky::ui::StackSlot slot;
            slot.width  = sky::ui::StackSize::Fill;
            slot.height = sky::ui::StackSize::Fixed;
            slot.fixedH = kRowH;
            rowsPanel->Add(std::make_unique<sky::ui::UIElement>(), slot);
        }
        MarkPaintDirty();
    }

    void MaterialEditorView::LayoutRows()
    {
        if (rowsPanel == nullptr) {
            return;
        }
        const sky::ui::UIRect bounds = GetBounds();
        const float           top    = bounds.top + kPad + kTitleH;
        rowsPanel->SetBounds(sky::ui::UIRect{bounds.left, top, bounds.right, top + static_cast<float>(rows.size()) * kRowH});
        rowsPanel->Layout();
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
        const sky::ui::UIRect bounds = GetBounds();
        uc::Fill(context, bounds, uc::color::Panel);
        uc::Text(context, title, kTextSz, sky::ui::UIRect{bounds.left + kPad, bounds.top + kPad, bounds.right - kPad, bounds.top + kPad + kTitleH},
                 uc::color::TextMuted, text);
        uc::HLine(context, bounds.left + kPad, bounds.right - kPad, bounds.top + kPad + kTitleH, uc::color::BorderSoft);

        LayoutRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            const MaterialEditorRow &prop = rows[i];
            const sky::ui::UIRect    row  = RowRect(i);
            if (row.bottom < bounds.top || row.top > bounds.bottom) {
                continue;
            }
            if (i % 2 == 1) {
                uc::Fill(context, row, uc::color::RowOdd);
            }

            if (prop.kind == MaterialRowKind::TechniqueHeader) {
                uc::Fill(context, row, uc::color::RowHover);
                uc::Text(context, prop.name, kTextSz, sky::ui::UIRect{row.left + kPad, row.top, row.right - kPad - kBrowseW, row.bottom},
                         uc::color::Text, text);
                const bool            delHover = (hoverRow == static_cast<int>(i));
                const sky::ui::UIRect del{row.right - kPad - kBrowseW, row.top + 1.0f, row.right - kPad, row.bottom - 1.0f};
                uc::RoundedField(context, del, uc::color::FieldHover, delHover ? uc::color::Border : uc::color::BorderSoft, 3.0f);
                uc::Text(context, "x", kTextSz, del, uc::color::Text, text, uc::HAlign::Center);
                continue;
            }
            if (prop.kind == MaterialRowKind::AddTechnique) {
                const bool addHover = (hoverRow == static_cast<int>(i));
                if (addHover) {
                    uc::Fill(context, row, uc::color::RowHover);
                }
                uc::Text(context, prop.name, kTextSz, sky::ui::UIRect{row.left + kPad, row.top, row.right - kPad, row.bottom},
                         addHover ? uc::color::Text : uc::color::TextMuted, text, uc::HAlign::Center);
                continue;
            }

            uc::Text(context, prop.name, kTextSz, sky::ui::UIRect{row.left + kPad, row.top, row.left + kPad + kLabelW, row.bottom}, uc::color::Text,
                     text);

            const bool            isTexture = IsTextureProperty(prop.type);
            const float           boxRight  = isTexture ? row.right - kPad - kBrowseW - 2.0f : row.right - kPad;
            const sky::ui::UIRect box{row.left + kPad + kLabelW, row.top + 1.0f, boxRight, row.bottom - 1.0f};

            if (isTexture) {
                uc::RoundedField(context, box, uc::color::Field, uc::color::BorderSoft, 3.0f);
                uc::Text(context, DisplayText(prop), kTextSz, sky::ui::UIRect{box.left + 5.0f, box.top, box.right - 5.0f, box.bottom},
                         prop.texture ? uc::color::Text : uc::color::TextDisabled, text);
                const sky::ui::UIRect browse{row.right - kPad - kBrowseW, row.top + 1.0f, row.right - kPad, row.bottom - 1.0f};
                uc::RoundedField(context, browse, uc::color::FieldHover, uc::color::BorderSoft, 3.0f);
                uc::Text(context, "...", kTextSz, browse, uc::color::Text, text, uc::HAlign::Center);
            } else {
                const bool editing = (editingRow == static_cast<int>(i));
                uc::RoundedField(context, box, editing ? uc::color::FieldHover : uc::color::Field,
                                 editing ? uc::color::Border : uc::color::BorderSoft, 3.0f);
                const std::string shown = editing ? editText : DisplayText(prop);
                const uint32_t    color = (prop.type == MaterialPropertyType::MAT4) ? uc::color::TextDisabled : uc::color::Text;
                uc::Text(context, shown, kTextSz, sky::ui::UIRect{box.left + 5.0f, box.top, box.right - 5.0f, box.bottom}, color, text);
                if (editing && text != nullptr) {
                    const float caretX = box.left + 5.0f + sky::ui::UITextLayout::Measure(shown, kTextSz, text->GetAtlas()).width + 1.0f;
                    uc::Fill(context, sky::ui::UIRect{caretX, box.top + 2.0f, caretX + 1.0f, box.bottom - 2.0f}, uc::color::Text);
                }
            }
        }

        if (pickerRow >= 0) {
            uc::SoftShadow(context, pickerRect, 4.0f);
            uc::RoundedField(context, pickerRect, uc::color::Panel, uc::color::Border, 4.0f);
            const sky::ui::UIRect noneRect{pickerRect.left + 4.0f, pickerRect.top + 4.0f, pickerRect.right - 4.0f, pickerRect.top + 4.0f + kRowH};
            uc::RoundedField(context, noneRect, uc::color::RowHover, uc::color::BorderSoft, 3.0f);
            uc::Text(context, "(none)", kTextSz, sky::ui::UIRect{noneRect.left + 6.0f, noneRect.top, noneRect.right - 6.0f, noneRect.bottom},
                     uc::color::TextMuted, text);
            for (size_t i = 0; i < pickerRects.size(); ++i) {
                if (static_cast<int>(i) == pickerHover) {
                    uc::Fill(context, pickerRects[i], uc::color::RowHover);
                }
                const std::string &label = pickerItems[i].name.empty() ? pickerItems[i].uuid.ToString() : pickerItems[i].name;
                uc::Text(context, label, kTextSz,
                         sky::ui::UIRect{pickerRects[i].left + 10.0f, pickerRects[i].top, pickerRects[i].right - 6.0f, pickerRects[i].bottom},
                         uc::color::Text, text);
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
        std::vector<float> floats;
        switch (p.type) {
        case MaterialPropertyType::FLOAT: {
            const float v = std::strtof(editText.c_str(), nullptr);
            p.value.resize(sizeof(float));
            std::memcpy(p.value.data(), &v, sizeof(float));
            break;
        }
        case MaterialPropertyType::INT: {
            const int32_t v = static_cast<int32_t>(std::strtol(editText.c_str(), nullptr, 10));
            p.value.resize(sizeof(int32_t));
            std::memcpy(p.value.data(), &v, sizeof(int32_t));
            break;
        }
        case MaterialPropertyType::UINT: {
            const uint32_t v = static_cast<uint32_t>(std::strtoul(editText.c_str(), nullptr, 10));
            p.value.resize(sizeof(uint32_t));
            std::memcpy(p.value.data(), &v, sizeof(uint32_t));
            break;
        }
        case MaterialPropertyType::BOOL: {
            p.value.assign(1, (editText == "true" || editText == "1") ? 1u : 0u);
            break;
        }
        case MaterialPropertyType::FLOAT2:
        case MaterialPropertyType::FLOAT3:
        case MaterialPropertyType::FLOAT4: {
            const size_t n = SizeOfPropertyType(p.type) / sizeof(float);
            if (ParseFloats(editText, floats, n)) {
                p.value.resize(n * sizeof(float));
                std::memcpy(p.value.data(), floats.data(), p.value.size());
            }
            break;
        }
        default: return;
        }
        PersistProperty(index);
    }

    void MaterialEditorView::CommitTexture(int index, const Uuid &id)
    {
        if (index < 0 || index >= static_cast<int>(rows.size())) {
            return;
        }
        rows[index].texture = id;
        PersistProperty(index);
    }

    bool MaterialEditorView::PersistProperty(int index)
    {
        if (index < 0 || index >= static_cast<int>(rows.size())) {
            return false;
        }
        const MaterialEditorRow &prop = rows[index];

        const std::string abs = SourceAbsolutePath(asset);
        if (abs.empty()) {
            LOG_W(TAG, "no source path for asset %s; edit is in-memory only", asset.ToString().c_str());
            return false;
        }

        std::ifstream in(abs, std::ios::binary);
        if (!in) {
            LOG_W(TAG, "cannot open source %s", abs.c_str());
            return false;
        }
        std::stringstream buffer;
        buffer << in.rdbuf();
        in.close();

        rapidjson::Document doc;
        doc.Parse(buffer.str().c_str());
        if (doc.HasParseError() || !doc.IsObject()) {
            LOG_W(TAG, "source parse failed: %s", abs.c_str());
            return false;
        }
        auto &alloc = doc.GetAllocator();
        if (prop.isString) {
            if (!doc.HasMember("techniques") || !doc["techniques"].IsArray()) {
                return false;
            }
            rapidjson::Value &techs = doc["techniques"];
            if (prop.techniqueIndex < 0 || prop.techniqueIndex >= static_cast<int>(techs.Size()) || !techs[prop.techniqueIndex].IsObject()) {
                return false;
            }
            rapidjson::Value &tech = techs[prop.techniqueIndex];
            if (prop.techniqueField == 10) {
                if (!tech.HasMember("renderState") || !tech["renderState"].IsObject()) {
                    tech.RemoveMember("renderState");
                    tech.AddMember(rapidjson::Value("renderState", alloc), rapidjson::Value(rapidjson::kObjectType), alloc);
                }
                rapidjson::Value *target = &tech["renderState"];
                std::string       key    = prop.renderKey;
                if (const auto dot = key.find('.'); dot != std::string::npos) {
                    const std::string outer = key.substr(0, dot);
                    if (!target->HasMember(outer.c_str()) || !(*target)[outer.c_str()].IsObject()) {
                        target->RemoveMember(outer.c_str());
                        target->AddMember(rapidjson::Value(outer.c_str(), alloc), rapidjson::Value(rapidjson::kObjectType), alloc);
                    }
                    target = &(*target)[outer.c_str()];
                    key    = key.substr(dot + 1);
                }
                target->RemoveMember(key.c_str());
                rapidjson::Value value;
                if (prop.text == "true") {
                    value = rapidjson::Value(true);
                } else if (prop.text == "false") {
                    value = rapidjson::Value(false);
                } else {
                    char        *end = nullptr;
                    const double num = std::strtod(prop.text.c_str(), &end);
                    if (end != nullptr && *end == '\0' && !prop.text.empty()) {
                        value = rapidjson::Value(num);
                    } else {
                        value = rapidjson::Value(prop.text.c_str(), alloc);
                    }
                }
                target->AddMember(rapidjson::Value(key.c_str(), alloc), value, alloc);
            } else {
                const char *fieldKey = prop.techniqueField == 0 ? "tag" : (prop.techniqueField == 1 ? "shader" : "entry");
                tech.RemoveMember(fieldKey);
                tech.AddMember(rapidjson::Value(fieldKey, alloc), rapidjson::Value(prop.text.c_str(), alloc), alloc);
            }
        } else {
            if (!doc.HasMember("properties") || !doc["properties"].IsObject()) {
                doc.RemoveMember("properties");
                doc.AddMember("properties", rapidjson::Value(rapidjson::kObjectType), alloc);
            }
            rapidjson::Value &props = doc["properties"];
            props.RemoveMember(prop.name.c_str());
            rapidjson::Value key(prop.name.c_str(), alloc);

            if (IsTextureProperty(prop.type)) {
                if (prop.texture) {
                    props.AddMember(key, rapidjson::Value(prop.texture.ToString().c_str(), alloc), alloc);
                }
            } else if (prop.type == MaterialPropertyType::BOOL) {
                props.AddMember(key, rapidjson::Value(ReadPod<uint8_t>(prop.value) != 0), alloc);
            } else if (prop.type == MaterialPropertyType::INT) {
                props.AddMember(key, rapidjson::Value(ReadPod<int32_t>(prop.value)), alloc);
            } else if (prop.type == MaterialPropertyType::UINT) {
                props.AddMember(key, rapidjson::Value(ReadPod<uint32_t>(prop.value)), alloc);
            } else if (prop.type == MaterialPropertyType::FLOAT) {
                props.AddMember(key, rapidjson::Value(ReadPod<float>(prop.value)), alloc);
            } else {
                rapidjson::Value arr(rapidjson::kArrayType);
                const size_t     n =
                    (prop.type == MaterialPropertyType::MAT4) ? prop.value.size() / sizeof(float) : SizeOfPropertyType(prop.type) / sizeof(float);
                for (size_t i = 0; i < n; ++i) {
                    arr.PushBack(rapidjson::Value(ReadPod<float>(prop.value, i * sizeof(float))), alloc);
                }
                props.AddMember(key, arr, alloc);
            }
        }

        rapidjson::StringBuffer                    sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
        doc.Accept(writer);

        std::ofstream out(abs, std::ios::binary | std::ios::trunc);
        if (!out) {
            LOG_W(TAG, "cannot write source %s", abs.c_str());
            return false;
        }
        out.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
        out.close();

        // NOTE: TriggerCook disabled for crash bisect.
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
        const float top = row.bottom + 2.0f;
        const float h   = 8.0f + kRowH + static_cast<float>(pickerItems.size()) * kRowH;
        pickerRect      = sky::ui::UIRect{row.left + kPad + kLabelW, top, row.left + kPad + kLabelW + 220.0f, top + h};
        pickerRects.clear();
        for (size_t k = 0; k < pickerItems.size(); ++k) {
            const float iy = pickerRect.top + 4.0f + kRowH + static_cast<float>(k) * kRowH;
            pickerRects.push_back(sky::ui::UIRect{pickerRect.left + 4.0f, iy, pickerRect.right - 4.0f, iy + kRowH});
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
                const sky::ui::UIRect noneRect{pickerRect.left + 4.0f, pickerRect.top + 4.0f, pickerRect.right - 4.0f, pickerRect.top + 4.0f + kRowH};
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
            LayoutRows();
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
        LayoutRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            const sky::ui::UIRect row = RowRect(i);
            if (!row.Contains(event.x, event.y)) {
                continue;
            }
            if (rows[i].kind == MaterialRowKind::TechniqueHeader) {
                const sky::ui::UIRect del{row.right - kPad - kBrowseW, row.top + 1.0f, row.right - kPad, row.bottom - 1.0f};
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
                const sky::ui::UIRect browse{row.right - kPad - kBrowseW, row.top, row.right - kPad, row.bottom};
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
                std::vector<MaterialEditorRow> propRows;
                if (auto asset = AssetManager::Get()->LoadAsset<Material>(uuid)) {
                    asset->BlockUntilLoaded();
                    for (const auto &p : asset->Data().properties) {
                        propRows.push_back(MaterialEditorRow{p.name, p.type, p.value, p.texture});
                    }
                }
                return std::make_unique<MaterialEditorView>(text, uuid, "Material", std::move(propRows), /*enableTechniques=*/true);
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
                std::vector<MaterialEditorRow> rows;
                if (auto asset = AssetManager::Get()->LoadAsset<MaterialInstance>(uuid)) {
                    asset->BlockUntilLoaded();
                    for (const auto &v : asset->Data().valueOverrides) {
                        rows.push_back(MaterialEditorRow{v.name, v.type, v.value, Uuid{}});
                    }
                    for (const auto &t : asset->Data().textureOverrides) {
                        rows.push_back(MaterialEditorRow{t.name, MaterialPropertyType::TEXTURE2D, {}, t.texture});
                    }
                }
                return std::make_unique<MaterialEditorView>(text, uuid, "Material Instance Overrides", std::move(rows));
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
