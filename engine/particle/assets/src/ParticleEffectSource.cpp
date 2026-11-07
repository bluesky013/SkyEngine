#include <particle/assets/ParticleEffectSource.h>

#include <particle/ParticleSchema.h>

#include <cmath>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace sky::particle {
    namespace {
        using Value  = rapidjson::Value;
        using Writer = rapidjson::Writer<rapidjson::StringBuffer>;

        struct Reader {
            std::string error;

            bool Fail(const char *name)
            {
                if (error.empty())
                    error = std::string("Invalid or missing Particle field: ") + name;
                return false;
            }
            const Value *Get(const Value &object, const char *key)
            {
                if (!object.IsObject())
                    return Fail(key), nullptr;
                const auto found = object.FindMember(key);
                return found == object.MemberEnd() ? (Fail(key), nullptr) : &found->value;
            }
            bool String(const Value &object, const char *key, std::string &out)
            {
                const auto *value = Get(object, key);
                if (!value || !value->IsString() || value->GetStringLength() > 256)
                    return Fail(key);
                if (std::string_view(value->GetString(), value->GetStringLength()).find('\0') != std::string_view::npos)
                    return Fail(key);
                out.assign(value->GetString(), value->GetStringLength());
                return true;
            }
            bool Float(const Value &object, const char *key, float &out)
            {
                const auto *value = Get(object, key);
                if (!value || !value->IsNumber() || !std::isfinite(value->GetFloat()))
                    return Fail(key);
                out = value->GetFloat();
                return true;
            }
            bool Uint(const Value &object, const char *key, uint32_t &out)
            {
                const auto *value = Get(object, key);
                if (!value || !value->IsUint())
                    return Fail(key);
                out = value->GetUint();
                return true;
            }
            bool Bool(const Value &object, const char *key, bool &out)
            {
                const auto *value = Get(object, key);
                if (!value || !value->IsBool())
                    return Fail(key);
                out = value->GetBool();
                return true;
            }
        };

        bool ParseVector(const Value &value, Vector3 &out)
        {
            if (!value.IsArray() || value.Size() != 3)
                return false;
            for (uint32_t i = 0; i < 3; ++i) {
                if (!value[i].IsNumber() || !std::isfinite(value[i].GetFloat()))
                    return false;
                out.v[i] = value[i].GetFloat();
            }
            return true;
        }

        bool ParseColor(const Value &value, Color &out)
        {
            if (!value.IsArray() || value.Size() != 4)
                return false;
            for (uint32_t i = 0; i < 4; ++i) {
                if (!value[i].IsNumber() || !std::isfinite(value[i].GetFloat()))
                    return false;
                out.v[i] = value[i].GetFloat();
            }
            return true;
        }

        bool ParseBounds(const Value &value, ParticleBounds &out)
        {
            if (!value.IsObject())
                return false;
            const auto min = value.FindMember("min");
            const auto max = value.FindMember("max");
            return min != value.MemberEnd() && max != value.MemberEnd() && ParseVector(min->value, out.min) && ParseVector(max->value, out.max) &&
                   out.Valid();
        }

        void WriteVector(Writer &writer, const Vector3 &value)
        {
            writer.StartArray();
            for (const auto channel : value.v)
                writer.Double(channel);
            writer.EndArray();
        }

        void WriteValue(Writer &writer, const ParticleValue &value)
        {
            writer.StartObject();
            writer.Key("kind");
            writer.Uint(static_cast<uint32_t>(value.kind));
            writer.Key("constant");
            writer.Double(value.constant);
            writer.Key("vector");
            WriteVector(writer, value.vector);
            writer.Key("min");
            writer.Double(value.range.min);
            writer.Key("max");
            writer.Double(value.range.max);
            writer.Key("parameter");
            const auto parameter = value.parameter.GetStr();
            writer.String(parameter.data(), static_cast<rapidjson::SizeType>(parameter.size()));
            writer.Key("attribute");
            writer.Uint(static_cast<uint32_t>(value.attribute));
            writer.Key("curve");
            writer.StartArray();
            for (const auto &key : value.curve.keys) {
                writer.StartObject();
                writer.Key("time");
                writer.Double(key.time);
                writer.Key("value");
                WriteVector(writer, key.value);
                writer.EndObject();
            }
            writer.EndArray();
            writer.Key("gradient");
            writer.StartArray();
            for (const auto &key : value.gradient.keys) {
                writer.StartObject();
                writer.Key("time");
                writer.Double(key.time);
                writer.Key("color");
                writer.StartArray();
                for (const auto channel : key.color.v)
                    writer.Double(channel);
                writer.EndArray();
                writer.EndObject();
            }
            writer.EndArray();
            writer.EndObject();
        }

        ParticleRendererType RendererType(std::string_view text)
        {
            if (text == "SPRITE")
                return ParticleRendererType::SPRITE;
            if (text == "MESH")
                return ParticleRendererType::MESH;
            return ParticleRendererType::END;
        }

        ParticleBlendMode BlendMode(std::string_view text)
        {
            if (text == "ADDITIVE")
                return ParticleBlendMode::ADDITIVE;
            if (text == "ALPHA")
                return ParticleBlendMode::ALPHA;
            if (text == "PREMULTIPLIED")
                return ParticleBlendMode::PREMULTIPLIED;
            return ParticleBlendMode::END;
        }

        bool ParseLegacyValue(const Value &value, ParticleValue &out)
        {
            out = {};
            if (value.IsNumber()) {
                out.kind     = ParticleValueKind::CONSTANT;
                out.constant = value.GetFloat();
                return std::isfinite(out.constant);
            }
            if (value.IsObject()) {
                const auto min = value.FindMember("min");
                const auto max = value.FindMember("max");
                if (min != value.MemberEnd() && max != value.MemberEnd() && min->value.IsNumber() && max->value.IsNumber()) {
                    out.kind      = ParticleValueKind::RANDOM;
                    out.range.min = min->value.GetFloat();
                    out.range.max = max->value.GetFloat();
                    return true;
                }
                return false;
            }
            if (!value.IsArray() || value.Empty())
                return false;
            if (value[0].IsNumber()) {
                Vector3 vector;
                if (!ParseVector(value, vector))
                    return false;
                out.kind   = ParticleValueKind::CONSTANT;
                out.vector = vector;
                return true;
            }
            if (!value[0].IsObject())
                return false;
            const bool gradient = value[0].HasMember("color");
            out.kind            = gradient ? ParticleValueKind::GRADIENT : ParticleValueKind::CURVE;
            for (const auto &key : value.GetArray()) {
                if (!key.IsObject() || !key.HasMember("time") || !key["time"].IsNumber())
                    return false;
                const auto time = key["time"].GetFloat();
                if (gradient) {
                    Color color;
                    if (!key.HasMember("color") || !ParseColor(key["color"], color))
                        return false;
                    out.gradient.keys.push_back({time, color});
                } else {
                    if (!key.HasMember("value") || !key["value"].IsNumber())
                        return false;
                    out.curve.keys.push_back({time, Vector3{key["value"].GetFloat(), 0.f, 0.f}});
                }
            }
            return !out.curve.keys.empty() || !out.gradient.keys.empty();
        }

        bool ParseValue(const Value &value, ParticleValue &out)
        {
            if (!value.IsObject() || !value.HasMember("kind")) {
                if (!ParseLegacyValue(value, out))
                    return false;
                std::string error;
                return ValidateParticleValue(out, error);
            }
            Reader      reader;
            uint32_t    kind = 0, attribute = 0;
            std::string parameter;
            if (!reader.Uint(value, "kind", kind) || kind >= static_cast<uint32_t>(ParticleValueKind::END) ||
                !reader.Uint(value, "attribute", attribute) || attribute >= PARTICLE_ATTRIBUTE_COUNT ||
                !reader.Float(value, "constant", out.constant) || !reader.Float(value, "min", out.range.min) ||
                !reader.Float(value, "max", out.range.max) || !reader.String(value, "parameter", parameter))
                return false;
            const auto *vector   = reader.Get(value, "vector");
            const auto *curve    = reader.Get(value, "curve");
            const auto *gradient = reader.Get(value, "gradient");
            if (!vector || !ParseVector(*vector, out.vector) || !curve || !curve->IsArray() || curve->Size() > MAX_PARTICLE_KEYS || !gradient ||
                !gradient->IsArray() || gradient->Size() > MAX_PARTICLE_KEYS)
                return false;
            out.kind      = static_cast<ParticleValueKind>(kind);
            out.attribute = static_cast<ParticleAttribute>(attribute);
            out.parameter = Name(parameter.c_str());
            for (const auto &key : curve->GetArray()) {
                ParticleCurveKey parsed;
                if (!reader.Float(key, "time", parsed.time))
                    return false;
                const auto *data = reader.Get(key, "value");
                if (!data || !ParseVector(*data, parsed.value))
                    return false;
                out.curve.keys.push_back(parsed);
            }
            for (const auto &key : gradient->GetArray()) {
                ParticleGradientKey parsed;
                if (!reader.Float(key, "time", parsed.time))
                    return false;
                const auto *data = reader.Get(key, "color");
                if (!data || !ParseColor(*data, parsed.color))
                    return false;
                out.gradient.keys.push_back(parsed);
            }
            std::string error;
            return ValidateParticleValue(out, error);
        }

        bool ParseModules(Reader &reader, const Value &array, std::vector<ParticleModuleDesc> &modules)
        {
            if (!array.IsArray() || array.Size() > 256)
                return reader.Fail("modules");
            modules.clear();
            for (const auto &entry : array.GetArray()) {
                std::string type;
                if (!reader.String(entry, "type", type))
                    return false;
                ParticleModuleDesc module;
                module.type = ParseParticleModuleType(type);
                if (!IsValidModuleType(module.type))
                    return reader.Fail("module.type");
                if (entry.HasMember("name")) {
                    std::string name;
                    if (!reader.String(entry, "name", name))
                        return false;
                    module.name = Name(name.c_str());
                }
                const auto params = entry.FindMember("params");
                if (params != entry.MemberEnd()) {
                    if (!params->value.IsObject())
                        return reader.Fail("module.params");
                    for (const auto &member : params->value.GetObject()) {
                        ParticleValue value;
                        if (!ParseValue(member.value, value) || member.name.GetStringLength() > 256)
                            return reader.Fail("module.param");
                        const auto            *schema = FindParticleModuleSchema(module.type);
                        const std::string_view parameterName(member.name.GetString(), member.name.GetStringLength());
                        if (parameterName.find('\0') != std::string_view::npos)
                            return reader.Fail("module.param.name");
                        for (const auto &parameter : schema->parameters) {
                            if (parameter.name != parameterName)
                                continue;
                            if (member.value.IsArray() && !member.value.Empty() && member.value[0].IsNumber() &&
                                parameter.shape != ParticleParameterShape::VECTOR)
                                return reader.Fail("module.param.shape");
                            if (member.value.IsNumber() && parameter.shape != ParticleParameterShape::SCALAR)
                                return reader.Fail("module.param.shape");
                        }
                        module.parameters.push_back({Name(std::string(member.name.GetString(), member.name.GetStringLength()).c_str()), value});
                    }
                }
                modules.push_back(std::move(module));
            }
            return true;
        }
    } // namespace

    static bool ParseParticleSourceData(std::string_view text, ParticleEffectDesc &effect, std::string &error)
    {
        error.clear();
        effect = {};
        if (text.empty() || text.size() > MAX_PARTICLE_SOURCE_BYTES) {
            error = "Particle source is empty or too large";
            return false;
        }
        rapidjson::Document document;
        if (document.Parse(text.data(), text.size()).HasParseError() || !document.IsObject()) {
            error = "Particle source is not valid JSON";
            return false;
        }
        Reader     reader;
        const auto emitters = document.FindMember("emitters");
        if (emitters == document.MemberEnd() || !emitters->value.IsArray() || emitters->value.Empty() || emitters->value.Size() > 64) {
            error = "Particle source requires bounded emitters";
            return false;
        }
        if (document.HasMember("version") && !reader.Uint(document, "version", effect.version)) {
            error = reader.error;
            return false;
        }
        if (document.HasMember("fixedBounds") && !ParseBounds(document["fixedBounds"], effect.fixedBounds)) {
            error = "Particle fixedBounds is invalid";
            return false;
        }
        if (document.HasMember("userParameters")) {
            const auto &params = document["userParameters"];
            if (!params.IsArray() || params.Size() > 256) {
                error = "Particle userParameters is invalid";
                return false;
            }
            for (const auto &entry : params.GetArray()) {
                ParticleUserParameter parameter;
                std::string           name;
                if (!reader.String(entry, "name", name) || !entry.HasMember("value") || !ParseValue(entry["value"], parameter.value)) {
                    error = "Particle user parameter is invalid";
                    return false;
                }
                parameter.name = Name(name.c_str());
                effect.userParameters.push_back(std::move(parameter));
            }
        }
        for (const auto &entry : emitters->value.GetArray()) {
            ParticleEmitterDesc emitter;
            std::string         name;
            if (!reader.String(entry, "name", name) || !reader.Uint(entry, "capacity", emitter.capacity))
                break;
            emitter.name = Name(name.c_str());
            if (entry.HasMember("bounds") && !ParseBounds(entry["bounds"], emitter.bounds)) {
                reader.Fail("bounds");
                break;
            }
            std::string target;
            if (!reader.String(entry, "simTarget", target))
                break;
            if (target != "CPU" && target != "GPU") {
                reader.Fail("simTarget");
                break;
            }
            emitter.simTarget = target == "GPU" ? ParticleSimTarget::GPU : ParticleSimTarget::CPU;
            if (entry.HasMember("space")) {
                std::string space;
                if (!reader.String(entry, "space", space))
                    break;
                if (space != "WORLD" && space != "LOCAL" && space != "CUSTOM") {
                    reader.Fail("space");
                    break;
                }
                emitter.space = space == "LOCAL" ? ParticleSimSpace::LOCAL : space == "CUSTOM" ? ParticleSimSpace::CUSTOM : ParticleSimSpace::WORLD;
            }
            const auto spawn = entry.FindMember("spawn");
            if (spawn != entry.MemberEnd()) {
                const auto &value = spawn->value;
                if (!value.IsObject()) {
                    reader.Fail("spawn");
                    break;
                }
                if ((value.HasMember("rate") && !reader.Float(value, "rate", emitter.spawn.rate)) ||
                    (value.HasMember("burstInterval") && !reader.Float(value, "burstInterval", emitter.spawn.burstInterval)) ||
                    (value.HasMember("burstCount") && !reader.Uint(value, "burstCount", emitter.spawn.burstCount)) ||
                    (value.HasMember("warmupSeconds") && !reader.Float(value, "warmupSeconds", emitter.spawn.warmupSeconds)))
                    break;
            }
            const auto modules = entry.FindMember("modules");
            if (modules != entry.MemberEnd() && !ParseModules(reader, modules->value, emitter.modules))
                break;
            const auto renderers = entry.FindMember("renderers");
            if (renderers != entry.MemberEnd()) {
                if (!renderers->value.IsArray() || renderers->value.Size() > 16) {
                    reader.Fail("renderers");
                    break;
                }
                for (const auto &value : renderers->value.GetArray()) {
                    std::string type, blend;
                    if (!reader.String(value, "type", type) || !reader.String(value, "blend", blend))
                        break;
                    ParticleRendererDesc renderer;
                    renderer.type  = RendererType(type);
                    renderer.blend = BlendMode(blend);
                    if (!IsValidRendererType(renderer.type) || renderer.blend >= ParticleBlendMode::END) {
                        reader.Fail("renderer.type/blend");
                        break;
                    }
                    if ((value.HasMember("lit") && !reader.Bool(value, "lit", renderer.lit)) ||
                        (value.HasMember("soft") && !reader.Bool(value, "soft", renderer.soft)))
                        break;
                    if (value.HasMember("texture") && !value["texture"].IsString()) {
                        reader.Fail("texture");
                        break;
                    }
                    if (value.HasMember("texture") && value["texture"].IsString() && value["texture"].GetStringLength() > 0) {
                        if (value["texture"].GetStringLength() != 36) {
                            reader.Fail("texture");
                            break;
                        }
                        const std::string id(value["texture"].GetString(), value["texture"].GetStringLength());
                        renderer.texture = Uuid::CreateFromString(id);
                        if (!renderer.texture) {
                            reader.Fail("texture");
                            break;
                        }
                    }
                    emitter.renderers.push_back(renderer);
                }
                if (!reader.error.empty())
                    break;
            }
            effect.emitters.push_back(std::move(emitter));
        }
        if (!reader.error.empty()) {
            error = reader.error;
            return false;
        }
        if (effect.emitters.size() != emitters->value.Size()) {
            error = "Particle emitter is incomplete";
            return false;
        }
        return true;
    }

    bool ParseParticleSource(std::string_view text, ParticleEffectDesc &effect, std::string &error)
    {
        ParticleEffectDesc next;
        if (!ParseParticleSourceData(text, next, error) || !ValidateParticleEffect(next, error))
            return false;
        effect = std::move(next);
        return true;
    }

    bool WriteParticleSource(const ParticleEffectDesc &effect, std::string &text)
    {
        std::string error;
        if (!ValidateParticleEffect(effect, error))
            return false;
        rapidjson::StringBuffer buffer;
        Writer                  writer(buffer);
        writer.StartObject();
        writer.Key("version");
        writer.Uint(effect.version);
        writer.Key("fixedBounds");
        writer.StartObject();
        writer.Key("min");
        writer.StartArray();
        for (const auto channel : effect.fixedBounds.min.v)
            writer.Double(channel);
        writer.EndArray();
        writer.Key("max");
        writer.StartArray();
        for (const auto channel : effect.fixedBounds.max.v)
            writer.Double(channel);
        writer.EndArray();
        writer.EndObject();
        writer.Key("userParameters");
        writer.StartArray();
        for (const auto &parameter : effect.userParameters) {
            writer.StartObject();
            writer.Key("name");
            const auto name = parameter.name.GetStr();
            writer.String(name.data(), static_cast<rapidjson::SizeType>(name.size()));
            writer.Key("value");
            WriteValue(writer, parameter.value);
            writer.EndObject();
        }
        writer.EndArray();
        writer.Key("emitters");
        writer.StartArray();
        for (const auto &emitter : effect.emitters) {
            writer.StartObject();
            writer.Key("name");
            const auto name = emitter.name.GetStr();
            writer.String(name.data(), static_cast<rapidjson::SizeType>(name.size()));
            writer.Key("simTarget");
            writer.String(emitter.simTarget == ParticleSimTarget::GPU ? "GPU" : "CPU");
            writer.Key("space");
            writer.String(emitter.space == ParticleSimSpace::LOCAL ? "LOCAL" : emitter.space == ParticleSimSpace::CUSTOM ? "CUSTOM" : "WORLD");
            writer.Key("bounds");
            writer.StartObject();
            writer.Key("min");
            WriteVector(writer, emitter.bounds.min);
            writer.Key("max");
            WriteVector(writer, emitter.bounds.max);
            writer.EndObject();
            writer.Key("capacity");
            writer.Uint(emitter.capacity);
            writer.Key("spawn");
            writer.StartObject();
            writer.Key("rate");
            writer.Double(emitter.spawn.rate);
            writer.Key("burstInterval");
            writer.Double(emitter.spawn.burstInterval);
            writer.Key("burstCount");
            writer.Uint(emitter.spawn.burstCount);
            writer.Key("warmupSeconds");
            writer.Double(emitter.spawn.warmupSeconds);
            writer.EndObject();
            writer.Key("modules");
            writer.StartArray();
            for (const auto &module : emitter.modules) {
                writer.StartObject();
                writer.Key("type");
                const auto *schema = FindParticleModuleSchema(module.type);
                writer.String(schema->token.data(), static_cast<rapidjson::SizeType>(schema->token.size()));
                writer.Key("name");
                const auto name = module.name.GetStr();
                writer.String(name.data(), static_cast<rapidjson::SizeType>(name.size()));
                writer.Key("params");
                writer.StartObject();
                for (const auto &parameter : module.parameters) {
                    const auto key = parameter.name.GetStr();
                    writer.Key(key.data(), static_cast<rapidjson::SizeType>(key.size()));
                    WriteValue(writer, parameter.value);
                }
                writer.EndObject();
                writer.EndObject();
            }
            writer.EndArray();
            writer.Key("renderers");
            writer.StartArray();
            for (const auto &renderer : emitter.renderers) {
                writer.StartObject();
                writer.Key("type");
                writer.String(renderer.type == ParticleRendererType::MESH ? "MESH" : "SPRITE");
                writer.Key("blend");
                writer.String(renderer.blend == ParticleBlendMode::ADDITIVE
                                  ? "ADDITIVE"
                                  : (renderer.blend == ParticleBlendMode::PREMULTIPLIED ? "PREMULTIPLIED" : "ALPHA"));
                writer.Key("lit");
                writer.Bool(renderer.lit);
                writer.Key("soft");
                writer.Bool(renderer.soft);
                if (renderer.texture) {
                    writer.Key("texture");
                    const auto id = renderer.texture.ToString();
                    writer.String(id.data(), static_cast<rapidjson::SizeType>(id.size()));
                }
                writer.EndObject();
            }
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
        if (!writer.IsComplete())
            return false;
        text.assign(buffer.GetString(), buffer.GetSize());
        return true;
    }
} // namespace sky::particle
