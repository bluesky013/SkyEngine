#include <curve/assets/CurveSource.h>

#include "CurveEnumSchema.h"

#include <cmath>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace sky::curve {
    namespace {
        using Value  = rapidjson::Value;
        using Writer = rapidjson::Writer<rapidjson::StringBuffer>;

        struct Reader {
            std::string error;

            bool Fail(const char *name)
            {
                if (error.empty())
                    error = std::string("Invalid or missing Curve field: ") + name;
                return false;
            }
            const Value *Get(const Value &object, const char *key)
            {
                if (!object.IsObject())
                    return Fail(key), nullptr;
                const auto found = object.FindMember(key);
                return found == object.MemberEnd() ? (Fail(key), nullptr) : &found->value;
            }
            bool Uint(const Value &object, const char *key, uint32_t &out)
            {
                const auto *value = Get(object, key);
                if (!value || !value->IsUint())
                    return Fail(key);
                out = value->GetUint();
                return true;
            }
            bool String(const Value &object, const char *key, std::string &out)
            {
                const auto *value = Get(object, key);
                if (!value || !value->IsString() || value->GetStringLength() > 32)
                    return Fail(key);
                out.assign(value->GetString(), value->GetStringLength());
                return true;
            }
        };

    } // namespace

    bool ParseCurveSource(std::string_view text, Curve &curve, std::string &error)
    {
        error.clear();
        if (text.empty() || text.size() > MAX_CURVE_SOURCE_BYTES) {
            error = "Curve source size is invalid";
            return false;
        }
        rapidjson::Document document;
        document.Parse<rapidjson::kParseIterativeFlag>(text.data(), text.size());
        if (document.HasParseError() || !document.IsObject()) {
            error = "Curve source is not valid JSON";
            return false;
        }
        Reader   reader;
        uint32_t version = 0;
        if (!reader.Uint(document, "version", version) || version != 1) {
            error = reader.error.empty() ? "Unsupported Curve source version" : reader.error;
            return false;
        }
        Curve next;
        if (!reader.Uint(document, "channels", next.channels)) {
            error = reader.error;
            return false;
        }
        if (next.channels < 1 || next.channels > 4) {
            error = "Curve channels must be in [1, 4]";
            return false;
        }
        std::string interpolation, preWrap, postWrap;
        if (!reader.String(document, "interpolation", interpolation) || !reader.String(document, "preWrap", preWrap) ||
            !reader.String(document, "postWrap", postWrap) || !detail::ParseEnum(interpolation, detail::INTERPOLATIONS, next.interpolation) ||
            !detail::ParseEnum(preWrap, detail::WRAPS, next.preWrap) || !detail::ParseEnum(postWrap, detail::WRAPS, next.postWrap)) {
            error = reader.error.empty() ? "Invalid Curve interpolation or wrap" : reader.error;
            return false;
        }
        const auto *keys = reader.Get(document, "keys");
        if (!keys || !keys->IsArray() || keys->Empty() || keys->Size() > MAX_CURVE_KEYS) {
            error = "Curve keys are missing or out of range";
            return false;
        }
        next.keys.reserve(keys->Size());
        for (const auto &entry : keys->GetArray()) {
            if (!entry.IsObject()) {
                error = "Curve key is not an object";
                return false;
            }
            CurveKey   key;
            const auto time  = entry.FindMember("time");
            const auto value = entry.FindMember("value");
            if (time == entry.MemberEnd() || !time->value.IsNumber() || !std::isfinite(time->value.GetFloat()) || value == entry.MemberEnd() ||
                !value->value.IsArray() || value->value.Size() != next.channels) {
                error = "Curve key time or value is invalid";
                return false;
            }
            key.time = time->value.GetFloat();
            for (rapidjson::SizeType i = 0; i < value->value.Size(); ++i) {
                const auto &component = value->value[i];
                if (!component.IsNumber() || !std::isfinite(component.GetFloat())) {
                    error = "Curve key component is invalid";
                    return false;
                }
                key.value.v[i] = component.GetFloat();
            }
            const auto readTangent = [&](const char *name, Vector4 &out) -> bool {
                const auto found = entry.FindMember(name);
                if (found == entry.MemberEnd())
                    return true;
                if (!found->value.IsArray() || found->value.Size() != next.channels)
                    return false;
                for (rapidjson::SizeType i = 0; i < found->value.Size(); ++i) {
                    if (!found->value[i].IsNumber() || !std::isfinite(found->value[i].GetFloat()))
                        return false;
                    out.v[i] = found->value[i].GetFloat();
                }
                return true;
            };
            if (!readTangent("tangentIn", key.tangentIn) || !readTangent("tangentOut", key.tangentOut)) {
                error = "Curve key tangent is invalid";
                return false;
            }
            next.keys.push_back(key);
        }
        if (!next.Validate(error))
            return false;
        curve = std::move(next);
        return true;
    }

    bool WriteCurveSource(const Curve &curve, std::string &text)
    {
        std::string error;
        if (!curve.Validate(error))
            return false;
        rapidjson::StringBuffer buffer;
        Writer                  writer(buffer);
        writer.StartObject();
        writer.Key("version");
        writer.Uint(1);
        writer.Key("channels");
        writer.Uint(curve.channels);
        writer.Key("interpolation");
        writer.String(detail::EnumToken(curve.interpolation, detail::INTERPOLATIONS));
        writer.Key("preWrap");
        writer.String(detail::EnumToken(curve.preWrap, detail::WRAPS));
        writer.Key("postWrap");
        writer.String(detail::EnumToken(curve.postWrap, detail::WRAPS));
        writer.Key("keys");
        writer.StartArray();
        for (const auto &key : curve.keys) {
            writer.StartObject();
            writer.Key("time");
            writer.Double(key.time);
            writer.Key("value");
            writer.StartArray();
            for (uint32_t i = 0; i < curve.channels; ++i)
                writer.Double(key.value.v[i]);
            writer.EndArray();
            writer.Key("tangentIn");
            writer.StartArray();
            for (uint32_t i = 0; i < curve.channels; ++i)
                writer.Double(key.tangentIn.v[i]);
            writer.EndArray();
            writer.Key("tangentOut");
            writer.StartArray();
            for (uint32_t i = 0; i < curve.channels; ++i)
                writer.Double(key.tangentOut.v[i]);
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
        text.assign(buffer.GetString(), buffer.GetSize());
        return true;
    }
} // namespace sky::curve
