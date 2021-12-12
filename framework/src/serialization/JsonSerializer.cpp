//
// Created by Zach Lee on 2021/12/11.
//

#include "JsonSerializer.h"
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <framework/serialization/SerializationContext.h>
#include <core/logger/Logger.h>

using namespace rapidjson;

static const char* TAG = "JsonSerializer";

namespace sky {

    using StrBuffer = rapidjson::StringBuffer;
    using Writer = rapidjson::PrettyWriter<rapidjson::StringBuffer>;
    using ValueType = rapidjson::Document::ValueType;

    static std::string_view GetTypeID(const TypeInfoRT* info)
    {
        if (info->name == TypeInfo<int32_t>::Name()) {
            return "int32_t";
        } else if (info->name == TypeInfo<int8_t>::Name()) {
            return "int8_t";
        } else if (info->name == TypeInfo<int16_t>::Name()) {
            return "int16_t";
        } else if (info->name == TypeInfo<uint32_t>::Name()) {
            return "uint32_t";
        } else if (info->name == TypeInfo<uint8_t>::Name()) {
            return "uint8_t";
        } else if (info->name == TypeInfo<uint16_t>::Name()) {
            return "uint16_t";
        } else if (info->name == TypeInfo<int64_t>::Name()) {
            return "int64_t";
        } else if (info->name == TypeInfo<uint64_t>::Name()) {
            return "uint64_t";
        } else if (info->name == TypeInfo<float>::Name()) {
            return "float";
        } else if (info->name == TypeInfo<double>::Name()) {
            return "double";
        } else if (info->name == TypeInfo<bool>::Name()) {
            return "bool";
        } else {
            return info->typeId;
        }
    }

    static void WriteFundamental(Writer& writer, const Any& any)
    {
        auto rtInfo = any.Info();
        if (rtInfo->name == TypeInfo<int32_t>::Name() ||
            rtInfo->name == TypeInfo<int8_t>::Name() ||
            rtInfo->name == TypeInfo<int16_t>::Name()) {
            writer.Int(*any.GetAsConst<int32_t>());
        } else if (rtInfo->name == TypeInfo<uint32_t>::Name() ||
            rtInfo->name == TypeInfo<uint8_t>::Name() ||
            rtInfo->name == TypeInfo<uint16_t>::Name()) {
            writer.Uint(*any.GetAsConst<uint32_t>());
        } else if (rtInfo->name == TypeInfo<int64_t>::Name()) {
            writer.Int64(*any.GetAsConst<int64_t>());
        } else if (rtInfo->name == TypeInfo<uint64_t>::Name()) {
            writer.Uint64(*any.GetAsConst<uint64_t>());
        } else if (rtInfo->name == TypeInfo<float>::Name()) {
            writer.Double(*any.GetAsConst<float>());
        } else if (rtInfo->name == TypeInfo<double>::Name()) {
            writer.Double(*any.GetAsConst<double>());
        } else if (rtInfo->name == TypeInfo<bool>::Name()) {
            writer.Bool(*any.GetAsConst<bool>());
        }
    }

    static void WriteObject(Writer& writer, const Any& any)
    {
        auto rtInfo = any.Info();
        writer.Key("ClassId");
        writer.String(GetTypeID(rtInfo).data());
        std::string val(rtInfo->name);

        auto context =SerializationContext::Get();
        auto node = context->FindType(rtInfo->typeId.data());

        writer.Key("Data");

        if (rtInfo->isFundamental) {
            WriteFundamental(writer, any);
        } else {
            writer.StartObject();
            for (auto& member : node->members) {
                writer.Key(member.first.data());
                if (member.second.info->isFundamental) {
                    WriteFundamental(writer, member.second.getterFn(any));
                } else {
                    writer.StartObject();
                    WriteObject(writer, member.second.getterFn(any));
                    writer.EndObject();
                }
            }
            writer.EndObject();
        }
    }

    void JsonSerializer::WriteString(const Any& any, std::string& output)
    {
        StrBuffer buffer;
        Writer writer(buffer);

        writer.StartObject();
        WriteObject(writer, any);
        writer.EndObject();

        output = buffer.GetString();
    }

    void JsonSerializer::ReadString(Any& any, const std::string& input)
    {
        Document document;
        document.Parse(input.data());

        if (document.HasParseError()) {
            LOG_E(TAG, "parse json failed, %u", document.GetParseError());
            return;
        }

        auto id = document.FindMember("ClassId");
        if (id == document.MemberEnd()) {
            return;
        }
        LOG_I(TAG, "parse class... %s", id->value.GetString());
    }

}