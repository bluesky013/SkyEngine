//
// Created by blues on 2026/10/3.
//

#include <framework/asset/CookProtocol.h>

#include <cstring>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace sky {

    namespace {

        constexpr const char *TYPE_NAMES[] = {
            "unknown", "hello", "ready", "cook", "result", "ping", "pong", "shutdown"
        };

        CookMessageType TypeFromName(const char *name)
        {
            for (uint32_t i = 0; i < sizeof(TYPE_NAMES) / sizeof(TYPE_NAMES[0]); ++i) {
                if (std::strcmp(name, TYPE_NAMES[i]) == 0) {
                    return static_cast<CookMessageType>(i);
                }
            }
            return CookMessageType::Unknown;
        }

    } // namespace

    const char *GetCookMessageTypeName(CookMessageType type)
    {
        const auto index = static_cast<uint32_t>(type);
        if (index < sizeof(TYPE_NAMES) / sizeof(TYPE_NAMES[0])) {
            return TYPE_NAMES[index];
        }
        return TYPE_NAMES[0];
    }

    std::string EncodeCookMessage(const CookMessage &message)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("type");
        writer.String(GetCookMessageTypeName(message.type));

        switch (message.type) {
            case CookMessageType::Hello:
                writer.Key("protocol");
                writer.Uint(message.protocol);
                break;
            case CookMessageType::Ready:
                writer.Key("protocol");
                writer.Uint(message.protocol);
                writer.Key("platform");
                writer.String(message.platform.c_str());
                break;
            case CookMessageType::Cook:
                writer.Key("id");
                writer.Uint64(message.id);
                writer.Key("uuid");
                writer.String(message.uuid.ToString().c_str());
                writer.Key("target");
                writer.String(message.target.c_str());
                writer.Key("path");
                writer.String(message.path.c_str());
                break;
            case CookMessageType::Result: {
                const std::string uuid = message.uuid.ToString();
                writer.Key("id");
                writer.Uint64(message.id);
                writer.Key("uuid");
                writer.String(uuid.c_str());
                writer.Key("target");
                writer.String(message.target.c_str());
                writer.Key("retCode");
                writer.Int(message.retCode);
                writer.Key("error");
                writer.String(message.error.c_str());
                break;
            }
            default:
                break;
        }

        writer.EndObject();
        return std::string(buffer.GetString(), buffer.GetSize());
    }

    bool DecodeCookMessage(const std::string &payload, CookMessage &out)
    {
        out = CookMessage{};

        rapidjson::Document document;
        document.Parse(payload.c_str(), payload.size());
        if (document.HasParseError() || !document.IsObject()) {
            return false;
        }

        const auto typeIt = document.FindMember("type");
        if (typeIt == document.MemberEnd() || !typeIt->value.IsString()) {
            return false;
        }
        out.type = TypeFromName(typeIt->value.GetString());
        if (out.type == CookMessageType::Unknown) {
            return false;
        }

        auto getString = [&document](const char *key, std::string &dst) {
            const auto it = document.FindMember(key);
            if (it != document.MemberEnd() && it->value.IsString()) {
                dst = it->value.GetString();
            }
        };
        auto getUint = [&document](const char *key, uint32_t &dst) {
            const auto it = document.FindMember(key);
            if (it != document.MemberEnd() && it->value.IsUint()) {
                dst = it->value.GetUint();
            }
        };
        auto getUint64 = [&document](const char *key, uint64_t &dst) {
            const auto it = document.FindMember(key);
            if (it != document.MemberEnd() && it->value.IsUint64()) {
                dst = it->value.GetUint64();
            }
        };
        auto getInt = [&document](const char *key, int32_t &dst) {
            const auto it = document.FindMember(key);
            if (it != document.MemberEnd() && it->value.IsInt()) {
                dst = it->value.GetInt();
            }
        };

        getUint("protocol", out.protocol);
        getUint64("id", out.id);
        getInt("retCode", out.retCode);
        getString("target", out.target);
        getString("path", out.path);
        getString("error", out.error);
        getString("platform", out.platform);

        std::string uuid;
        getString("uuid", uuid);
        if (!uuid.empty()) {
            out.uuid = Uuid::CreateFromString(uuid);
        }

        return true;
    }

} // namespace sky
