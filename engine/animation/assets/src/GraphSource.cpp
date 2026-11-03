#include <animation/assets/GraphAsset.h>
#include <cmath>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace sky::animation {
    namespace {
        constexpr size_t MAX_GRAPH_BYTES = 8u * 1024u * 1024u;
        using Value                      = rapidjson::Value;
        using Writer                     = rapidjson::Writer<rapidjson::StringBuffer>;
        GraphValidation Invalid(std::string text)
        {
            return {{{{}, "", std::move(text)}}};
        }
        struct Reader {
            GraphValidation status;
            void            Fail(const char *name)
            {
                if (status) {
                    status = Invalid(std::string("Invalid or missing Graph field: ") + name);
                }
            }
            const Value *Get(const Value &object, const char *key)
            {
                if (!object.IsObject()) {
                    Fail(key);
                    return nullptr;
                }
                const auto found = object.FindMember(key);
                if (found == object.MemberEnd()) {
                    Fail(key);
                    return nullptr;
                }
                return &found->value;
            }
            void Read(const Value &object, const char *key, Uuid &out)
            {
                const auto *v = Get(object, key);
                if (!v) {
                    return;
                }
                if (!v->IsString() || v->GetStringLength() != 36) {
                    Fail(key);
                    return;
                }
                const std::string text(v->GetString(), v->GetStringLength());
                const auto        id = Uuid::CreateFromString(text);
                if (text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-' || (!id && text != Uuid{}.ToString())) {
                    Fail(key);
                    return;
                }
                out = id;
            }
            void Read(const Value &object, const char *key, std::string &out)
            {
                const auto *v = Get(object, key);
                if (!v) {
                    return;
                }
                if (!v->IsString() || v->GetStringLength() > 256) {
                    Fail(key);
                    return;
                }
                out.assign(v->GetString(), v->GetStringLength());
            }
            void Read(const Value &object, const char *key, bool &out)
            {
                const auto *v = Get(object, key);
                if (!v) {
                    return;
                }
                if (!v->IsBool()) {
                    Fail(key);
                    return;
                }
                out = v->GetBool();
            }
            void Read(const Value &object, const char *key, float &out)
            {
                const auto *v = Get(object, key);
                if (!v) {
                    return;
                }
                if (!v->IsNumber() || !std::isfinite(v->GetDouble()) || std::abs(v->GetDouble()) > std::numeric_limits<float>::max()) {
                    Fail(key);
                    return;
                }
                out = static_cast<float>(v->GetDouble());
            }
            void Read(const Value &object, const char *key, uint32_t &out)
            {
                const auto *v = Get(object, key);
                if (!v) {
                    return;
                }
                if (!v->IsUint()) {
                    Fail(key);
                    return;
                }
                out = v->GetUint();
            }
            void Read(const Value &object, const char *key, int32_t &out)
            {
                const auto *v = Get(object, key);
                if (!v) {
                    return;
                }
                if (!v->IsInt()) {
                    Fail(key);
                    return;
                }
                out = v->GetInt();
            }
            void Read(const Value &object, const char *key, AnimationParam &out)
            {
                const auto *v = Get(object, key);
                if (!v) {
                    return;
                }
                if (v->IsBool()) {
                    out = v->GetBool();
                    return;
                }
                float value = 0.f;
                Read(object, key, value);
                out = value;
            }
            template <class T>
            void Enum(const Value &object, const char *key, T &out)
            {
                uint32_t raw = 0;
                Read(object, key, raw);
                if (raw > 255) {
                    Fail(key);
                    return;
                }
                out = static_cast<T>(raw);
            }
        };
        void Put(Writer &writer, const Uuid &id)
        {
            const auto text = id.ToString();
            writer.String(text.data(), static_cast<rapidjson::SizeType>(text.size()));
        }
        void Put(Writer &writer, const std::string &value)
        {
            writer.String(value.data(), static_cast<rapidjson::SizeType>(value.size()));
        }
        void Put(Writer &writer, bool value)
        {
            writer.Bool(value);
        }
        void Put(Writer &writer, float value)
        {
            writer.Double(value);
        }
        void Put(Writer &writer, uint32_t value)
        {
            writer.Uint(value);
        }
        void Put(Writer &writer, int32_t value)
        {
            writer.Int(value);
        }
        void Put(Writer &writer, const AnimationParam &value)
        {
            std::visit([&](const auto &item) { Put(writer, item); }, value);
        }
        void Read(Reader &reader, const Value &object, GraphParameter &out);
        void Put(Writer &writer, const GraphParameter &value);
        void Read(Reader &reader, const Value &object, GraphNode &out);
        void Put(Writer &writer, const GraphNode &value);
        void Read(Reader &reader, const Value &object, GraphLink &out);
        void Put(Writer &writer, const GraphLink &value);
        void Read(Reader &reader, const Value &object, GraphState &out);
        void Put(Writer &writer, const GraphState &value);
        void Read(Reader &reader, const Value &object, GraphCondition &out);
        void Put(Writer &writer, const GraphCondition &value);
        void Read(Reader &reader, const Value &object, GraphTransition &out);
        void Put(Writer &writer, const GraphTransition &value);
        void Read(Reader &reader, const Value &object, GraphNodePosition &out);
        void Put(Writer &writer, const GraphNodePosition &value);
        void Read(Reader &reader, const Value &object, GraphEditorLayout &out);
        void Put(Writer &writer, const GraphEditorLayout &value);
        void Read(Reader &reader, const Value &object, GraphDefinition &out);
        void Put(Writer &writer, const GraphDefinition &value);
        template <class T>
        void ReadArray(Reader &reader, const Value &object, const char *key, std::vector<T> &out, size_t limit)
        {
            const auto *array = reader.Get(object, key);
            if (!array) {
                return;
            }
            if (!array->IsArray() || array->Size() > limit) {
                reader.Fail(key);
                return;
            }
            out.resize(array->Size());
            for (size_t i = 0; i < out.size(); ++i) {
                Read(reader, (*array)[static_cast<rapidjson::SizeType>(i)], out[i]);
            }
        }
        void Read(Reader &reader, const Value &object, GraphParameter &out)
        {
            reader.Read(object, "id", out.id);
            reader.Read(object, "name", out.name);
            reader.Enum(object, "type", out.type);
            reader.Read(object, "value", out.value);
        }
        void Put(Writer &writer, const GraphParameter &value)
        {
            writer.StartObject();
            writer.Key("id");
            Put(writer, value.id);
            writer.Key("name");
            Put(writer, value.name);
            writer.Key("type");
            writer.Uint(static_cast<uint32_t>(value.type));
            writer.Key("value");
            Put(writer, value.value);
            writer.EndObject();
        }
        void Read(Reader &reader, const Value &object, GraphNode &out)
        {
            reader.Read(object, "id", out.id);
            reader.Enum(object, "kind", out.kind);
            reader.Read(object, "clip", out.clip);
            reader.Read(object, "loop", out.loop);
            reader.Read(object, "rate", out.rate);
            reader.Read(object, "alpha", out.alpha);
            reader.Read(object, "alphaParameter", out.alphaParameter);
        }
        void Put(Writer &writer, const GraphNode &value)
        {
            writer.StartObject();
            writer.Key("id");
            Put(writer, value.id);
            writer.Key("kind");
            writer.Uint(static_cast<uint32_t>(value.kind));
            writer.Key("clip");
            Put(writer, value.clip);
            writer.Key("loop");
            Put(writer, value.loop);
            writer.Key("rate");
            Put(writer, value.rate);
            writer.Key("alpha");
            Put(writer, value.alpha);
            writer.Key("alphaParameter");
            Put(writer, value.alphaParameter);
            writer.EndObject();
        }
        void Read(Reader &reader, const Value &object, GraphLink &out)
        {
            reader.Read(object, "from", out.from);
            reader.Enum(object, "output", out.output);
            reader.Read(object, "to", out.to);
            reader.Enum(object, "input", out.input);
        }
        void Put(Writer &writer, const GraphLink &value)
        {
            writer.StartObject();
            writer.Key("from");
            Put(writer, value.from);
            writer.Key("output");
            writer.Uint(static_cast<uint32_t>(value.output));
            writer.Key("to");
            Put(writer, value.to);
            writer.Key("input");
            writer.Uint(static_cast<uint32_t>(value.input));
            writer.EndObject();
        }
        void Read(Reader &reader, const Value &object, GraphState &out)
        {
            reader.Read(object, "id", out.id);
            reader.Read(object, "name", out.name);
            reader.Read(object, "output", out.output);
        }
        void Put(Writer &writer, const GraphState &value)
        {
            writer.StartObject();
            writer.Key("id");
            Put(writer, value.id);
            writer.Key("name");
            Put(writer, value.name);
            writer.Key("output");
            Put(writer, value.output);
            writer.EndObject();
        }
        void Read(Reader &reader, const Value &object, GraphCondition &out)
        {
            reader.Read(object, "parameter", out.parameter);
            reader.Enum(object, "comparison", out.comparison);
            reader.Read(object, "value", out.value);
        }
        void Put(Writer &writer, const GraphCondition &value)
        {
            writer.StartObject();
            writer.Key("parameter");
            Put(writer, value.parameter);
            writer.Key("comparison");
            writer.Uint(static_cast<uint32_t>(value.comparison));
            writer.Key("value");
            Put(writer, value.value);
            writer.EndObject();
        }
        void Read(Reader &reader, const Value &object, GraphTransition &out)
        {
            reader.Read(object, "id", out.id);
            reader.Read(object, "from", out.from);
            reader.Read(object, "to", out.to);
            reader.Read(object, "priority", out.priority);
            reader.Read(object, "duration", out.duration);
            reader.Read(object, "hasExitTime", out.hasExitTime);
            reader.Read(object, "timeSourceNode", out.timeSourceNode);
            reader.Read(object, "exitTime", out.exitTime);
            ReadArray(reader, object, "conditions", out.conditions, 256);
        }
        void Put(Writer &writer, const GraphTransition &value)
        {
            writer.StartObject();
            writer.Key("id");
            Put(writer, value.id);
            writer.Key("from");
            Put(writer, value.from);
            writer.Key("to");
            Put(writer, value.to);
            writer.Key("priority");
            Put(writer, value.priority);
            writer.Key("duration");
            Put(writer, value.duration);
            writer.Key("hasExitTime");
            Put(writer, value.hasExitTime);
            writer.Key("timeSourceNode");
            Put(writer, value.timeSourceNode);
            writer.Key("exitTime");
            Put(writer, value.exitTime);
            writer.Key("conditions");
            writer.StartArray();
            for (const auto &item : value.conditions) {
                Put(writer, item);
            }
            writer.EndArray();
            writer.EndObject();
        }
        void Read(Reader &reader, const Value &object, GraphNodePosition &out)
        {
            reader.Read(object, "node", out.node);
            reader.Read(object, "x", out.x);
            reader.Read(object, "y", out.y);
        }
        void Put(Writer &writer, const GraphNodePosition &value)
        {
            writer.StartObject();
            writer.Key("node");
            Put(writer, value.node);
            writer.Key("x");
            Put(writer, value.x);
            writer.Key("y");
            Put(writer, value.y);
            writer.EndObject();
        }
        void Read(Reader &reader, const Value &object, GraphEditorLayout &out)
        {
            ReadArray(reader, object, "nodes", out.nodes, 4096);
            reader.Read(object, "panX", out.panX);
            reader.Read(object, "panY", out.panY);
            reader.Read(object, "zoom", out.zoom);
        }
        void Put(Writer &writer, const GraphEditorLayout &value)
        {
            writer.StartObject();
            writer.Key("nodes");
            writer.StartArray();
            for (const auto &item : value.nodes) {
                Put(writer, item);
            }
            writer.EndArray();
            writer.Key("panX");
            Put(writer, value.panX);
            writer.Key("panY");
            Put(writer, value.panY);
            writer.Key("zoom");
            Put(writer, value.zoom);
            writer.EndObject();
        }
        void Read(Reader &reader, const Value &object, GraphDefinition &out)
        {
            reader.Read(object, "version", out.version);
            reader.Read(object, "document", out.document);
            reader.Read(object, "skeleton", out.skeleton);
            reader.Read(object, "output", out.output);
            reader.Read(object, "entryState", out.entryState);
            ReadArray(reader, object, "parameters", out.parameters, 256);
            ReadArray(reader, object, "nodes", out.nodes, 4096);
            ReadArray(reader, object, "links", out.links, 8192);
            ReadArray(reader, object, "states", out.states, 256);
            ReadArray(reader, object, "transitions", out.transitions, 4096);
        }
        void Put(Writer &writer, const GraphDefinition &value)
        {
            writer.StartObject();
            writer.Key("version");
            Put(writer, value.version);
            writer.Key("document");
            Put(writer, value.document);
            writer.Key("skeleton");
            Put(writer, value.skeleton);
            writer.Key("output");
            Put(writer, value.output);
            writer.Key("entryState");
            Put(writer, value.entryState);
            writer.Key("parameters");
            writer.StartArray();
            for (const auto &item : value.parameters) {
                Put(writer, item);
            }
            writer.EndArray();
            writer.Key("nodes");
            writer.StartArray();
            for (const auto &item : value.nodes) {
                Put(writer, item);
            }
            writer.EndArray();
            writer.Key("links");
            writer.StartArray();
            for (const auto &item : value.links) {
                Put(writer, item);
            }
            writer.EndArray();
            writer.Key("states");
            writer.StartArray();
            for (const auto &item : value.states) {
                Put(writer, item);
            }
            writer.EndArray();
            writer.Key("transitions");
            writer.StartArray();
            for (const auto &item : value.transitions) {
                Put(writer, item);
            }
            writer.EndArray();
            writer.EndObject();
        }
    } // namespace
    GraphValidation ParseGraphSource(std::string_view text, GraphSource &out, const Uuid &legacyDocument)
    {
        if (text.empty() || text.size() > MAX_GRAPH_BYTES) {
            return Invalid("Graph source size is invalid");
        }
        rapidjson::Document document;
        document.Parse<rapidjson::kParseIterativeFlag>(text.data(), text.size());
        if (document.HasParseError() || !document.IsObject()) {
            return Invalid("Graph source must be a JSON object");
        }
        const bool emptyLegacy = document.MemberCount() == 0 || (document.MemberCount() == 1 && document.HasMember("version") &&
                                                                 document["version"].IsUint() && document["version"].GetUint() == 1);
        if (emptyLegacy) {
            GraphSource migrated;
            migrated.definition.document = legacyDocument ? legacyDocument : Uuid::Create();
            out                          = std::move(migrated);
            return {};
        }
        Reader      reader;
        GraphSource next;
        Read(reader, document, next.definition);
        if (document.HasMember("editor")) {
            Read(reader, document["editor"], next.editor);
        }
        if (!reader.status) {
            return reader.status;
        }
        auto status = ValidateGraphSource(next);
        if (status) {
            out = std::move(next);
        }
        return status;
    }
    GraphValidation WriteGraphSource(const GraphSource &source, std::string &out, bool includeEditor)
    {
        auto status = ValidateGraphSource(source);
        if (!status) {
            return status;
        }
        rapidjson::StringBuffer buffer;
        Writer                  writer(buffer);
        Put(writer, source.definition);
        if (includeEditor) {
            rapidjson::Document document;
            document.Parse(buffer.GetString(), buffer.GetSize());
            rapidjson::StringBuffer editorBuffer;
            Writer                  editorWriter(editorBuffer);
            Put(editorWriter, source.editor);
            rapidjson::Document editor;
            editor.Parse(editorBuffer.GetString(), editorBuffer.GetSize());
            Value layout;
            layout.CopyFrom(editor, document.GetAllocator());
            document.AddMember("editor", layout, document.GetAllocator());
            buffer.Clear();
            Writer combined(buffer);
            document.Accept(combined);
        }
        if (buffer.GetSize() > MAX_GRAPH_BYTES) {
            return Invalid("Graph source exceeds the size limit");
        }
        out.assign(buffer.GetString(), buffer.GetSize());
        return {};
    }
} // namespace sky::animation
