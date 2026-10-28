#include <framework/world/WorldAsset.h>

#include <core/math/Matrix4.h>
#include <core/math/Quaternion.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <framework/serialization/ArrayVisitor.h>
#include <framework/serialization/SerializationUtil.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/WorldDesc.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <rapidjson/document.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace sky {
    namespace {
        using Bytes                     = std::vector<uint8_t>;
        using Json                      = rapidjson::Value;
        constexpr uint32_t MAGIC        = 0x444C5257;
        constexpr size_t   VALUE_HEADER = 21;

        bool Fail(std::string &error, std::string message)
        {
            error = std::move(message);
            return false;
        }

        bool Utf8(std::string_view text)
        {
            size_t i = 0;
            while (i < text.size()) {
                const auto first = static_cast<uint8_t>(text[i++]);
                if (first < 0x80)
                    continue;
                uint32_t code    = 0;
                uint32_t count   = 0;
                uint32_t minimum = 0;
                if (first >= 0xC2 && first <= 0xDF) {
                    code    = first & 0x1F;
                    count   = 1;
                    minimum = 0x80;
                } else if (first >= 0xE0 && first <= 0xEF) {
                    code    = first & 0x0F;
                    count   = 2;
                    minimum = 0x800;
                } else if (first >= 0xF0 && first <= 0xF4) {
                    code    = first & 0x07;
                    count   = 3;
                    minimum = 0x10000;
                } else
                    return false;
                if (count > text.size() - i)
                    return false;
                while (count--) {
                    const auto next = static_cast<uint8_t>(text[i++]);
                    if ((next & 0xC0) != 0x80)
                        return false;
                    code = (code << 6) | (next & 0x3F);
                }
                if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
                    return false;
            }
            return true;
        }

        struct Writer {
            Bytes        bytes;
            std::string &error;
            explicit Writer(std::string &message) : error(message)
            {
            }
            bool Raw(std::span<const uint8_t> value)
            {
                if (value.size() > WorldAssetData::MAX_BYTES - bytes.size())
                    return Fail(error, "World binary byte budget exceeded");
                bytes.insert(bytes.end(), value.begin(), value.end());
                return true;
            }
            bool Integer(uint64_t value, size_t width)
            {
                std::array<uint8_t, 8> data{};
                for (size_t i = 0; i < width; ++i)
                    data[i] = static_cast<uint8_t>(value >> (i * 8));
                return Raw(std::span(data.data(), width));
            }
            bool Id(const Uuid &id)
            {
                return Raw(id.data);
            }
            bool Text(std::string_view value)
            {
                return (value.size() <= WorldAssetData::MAX_STRING && Utf8(value))
                           ? Integer(value.size(), 4) && Raw({reinterpret_cast<const uint8_t *>(value.data()), value.size()})
                           : Fail(error, "Invalid or oversized World string");
            }
            bool Blob(std::span<const uint8_t> value)
            {
                return Integer(value.size(), 4) && Raw(value);
            }
            void PatchLength(size_t position, size_t length)
            {
                for (size_t i = 0; i < 4; ++i)
                    bytes[position + i] = static_cast<uint8_t>(length >> (i * 8));
            }
        };

        struct Reader {
            std::span<const uint8_t> bytes;
            std::string             &error;
            size_t                   position = 0;
            Reader(std::span<const uint8_t> value, std::string &message) : bytes(value), error(message)
            {
            }
            size_t Remaining() const
            {
                return bytes.size() - position;
            }
            bool Take(size_t count, std::span<const uint8_t> &value)
            {
                if (count > Remaining())
                    return Fail(error, "Truncated World binary record");
                value = bytes.subspan(position, count);
                position += count;
                return true;
            }
            bool Integer(uint64_t &value, size_t width)
            {
                std::span<const uint8_t> data;
                if (!Take(width, data))
                    return false;
                value = 0;
                for (size_t i = 0; i < width; ++i)
                    value |= uint64_t(data[i]) << (i * 8);
                return true;
            }
            bool Id(Uuid &id)
            {
                std::span<const uint8_t> data;
                if (!Take(sizeof(id.data), data))
                    return false;
                std::memcpy(id.data, data.data(), data.size());
                return true;
            }
            bool Text(std::string_view &value)
            {
                uint64_t                 length = 0;
                std::span<const uint8_t> data;
                if (!Integer(length, 4) || length > WorldAssetData::MAX_STRING || !Take(length, data))
                    return Fail(error, "Invalid World string length");
                value = {reinterpret_cast<const char *>(data.data()), data.size()};
                return Utf8(value) || Fail(error, "World string is not UTF-8");
            }
            bool Text(std::string &value)
            {
                std::string_view text;
                if (!Text(text))
                    return false;
                value.assign(text);
                return true;
            }
            bool Blob(Bytes &value)
            {
                uint64_t                 length = 0;
                std::span<const uint8_t> data;
                if (!Integer(length, 4) || length > WorldAssetData::MAX_BYTES || !Take(length, data))
                    return Fail(error, "Invalid World payload length");
                value.assign(data.begin(), data.end());
                return true;
            }
            bool Count(uint32_t &value, uint32_t limit, size_t minimum)
            {
                uint64_t count = 0;
                if (!Integer(count, 4) || count > limit || (minimum && count > Remaining() / minimum))
                    return Fail(error, "World record count exceeds its bounds");
                value = static_cast<uint32_t>(count);
                return true;
            }
            bool End()
            {
                return Remaining() == 0 || Fail(error, "Trailing bytes in World binary record");
            }
        };

        struct Budget {
            uint32_t nodes = 0;
            bool     Enter(uint32_t depth, std::string &error)
            {
                return (depth <= WorldAssetData::MAX_DEPTH && ++nodes <= WorldAssetData::MAX_ELEMENTS) ||
                       Fail(error, "World typed-value depth or element budget exceeded");
            }
        };

        enum class Kind : uint8_t { INVALID, SCALAR, STRING, UUID, NAME, MATH, ENUM, OBJECT, SEQUENCE, ANY };

        uint32_t MathCount(const Uuid &id)
        {
            if (id == TypeInfo<Vector2>::RegisteredId())
                return 2;
            if (id == TypeInfo<Vector3>::RegisteredId())
                return 3;
            if (id == TypeInfo<Vector4>::RegisteredId() || id == TypeInfo<Quaternion>::RegisteredId())
                return 4;
            if (id == TypeInfo<Matrix4>::RegisteredId())
                return 16;
            return 0;
        }

        Kind ValueKind(const TypeInfoRT *type)
        {
            if (!type || !type->staticInfo || type->staticInfo->isPointer || type->staticInfo->isAssociativeContainer)
                return Kind::INVALID;
            const auto &id = type->registeredId;
            if (id && id == TypeInfo<std::string>::RegisteredId())
                return Kind::STRING;
            if (id && id == TypeInfo<Uuid>::RegisteredId())
                return Kind::UUID;
            if (id && id == TypeInfo<Name>::RegisteredId())
                return Kind::NAME;
            if (id && id == TypeInfo<Any>::RegisteredId())
                return Kind::ANY;
            if (id && MathCount(id))
                return Kind::MATH;
            if (type->staticInfo->isArithmetic && id)
                return Kind::SCALAR;
            if (type->containerInfo && type->containerInfo->sequenceView && type->containerInfo->valueType)
                return Kind::SEQUENCE;
            const auto *node = id ? GetTypeNode(id) : nullptr;
            if (node && type->staticInfo->isEnum)
                return Kind::ENUM;
            return node && !node->members.empty() ? Kind::OBJECT : Kind::INVALID;
        }

        const TypeInfoRT *FindType(const Uuid &id)
        {
            const auto *node = id ? GetTypeNode(id) : nullptr;
            return node ? node->info : nullptr;
        }

        bool BeginValue(Writer &writer, const TypeInfoRT *type, Kind &kind, size_t &position)
        {
            kind = ValueKind(type);
            if (kind == Kind::INVALID)
                return Fail(writer.error, "World contains an unsupported or unregistered persistence schema");
            if (!writer.Integer(static_cast<uint8_t>(kind), 1) || !writer.Id(type->registeredId))
                return false;
            position = writer.bytes.size();
            return writer.Integer(0, 4);
        }

        template <class T>
        bool WriteScalar(Writer &writer, T value)
        {
            if constexpr (std::is_same_v<T, bool>)
                return writer.Integer(value ? 1 : 0, 1);
            else if constexpr (std::is_floating_point_v<T>) {
                if (!std::isfinite(value))
                    return Fail(writer.error, "Non-finite World scalar");
                if constexpr (sizeof(T) == 4)
                    return writer.Integer(std::bit_cast<uint32_t>(value), 4);
                else
                    return writer.Integer(std::bit_cast<uint64_t>(value), 8);
            } else
                return writer.Integer(static_cast<std::make_unsigned_t<T>>(value), sizeof(T));
        }

        template <class T>
        bool ReadScalar(Reader &reader, void *object)
        {
            uint64_t bits = 0;
            if (!reader.Integer(bits, sizeof(T)))
                return false;
            T value{};
            if constexpr (std::is_same_v<T, bool>) {
                if (bits > 1)
                    return Fail(reader.error, "Invalid World boolean");
                value = bits != 0;
            } else if constexpr (std::is_floating_point_v<T>) {
                if constexpr (sizeof(T) == 4)
                    value = std::bit_cast<T>(static_cast<uint32_t>(bits));
                else
                    value = std::bit_cast<T>(bits);
                if (!std::isfinite(value))
                    return Fail(reader.error, "Non-finite World scalar");
            } else
                value = std::bit_cast<T>(static_cast<std::make_unsigned_t<T>>(bits));
            if (object)
                *static_cast<T *>(object) = value;
            return true;
        }

        template <class T>
        bool JsonScalar(const Json &json, Writer &writer)
        {
            if constexpr (std::is_same_v<T, bool>) {
                return json.IsBool() ? WriteScalar(writer, json.GetBool()) : Fail(writer.error, "Expected World boolean");
            } else if constexpr (std::is_floating_point_v<T>) {
                if (!json.IsNumber() || !std::isfinite(json.GetDouble()) || std::abs(json.GetDouble()) > std::numeric_limits<T>::max())
                    return Fail(writer.error, "World floating-point value is out of range");
                return WriteScalar(writer, static_cast<T>(json.GetDouble()));
            } else if constexpr (std::is_signed_v<T>) {
                if (!json.IsInt64() || json.GetInt64() < std::numeric_limits<T>::min() || json.GetInt64() > std::numeric_limits<T>::max())
                    return Fail(writer.error, "World signed integer is out of range");
                return WriteScalar(writer, static_cast<T>(json.GetInt64()));
            } else {
                if (!json.IsUint64() || json.GetUint64() > std::numeric_limits<T>::max())
                    return Fail(writer.error, "World unsigned integer is out of range");
                return WriteScalar(writer, static_cast<T>(json.GetUint64()));
            }
        }

#define WORLD_SCALARS(OP)                                                                                                                            \
    OP(bool) OP(uint64_t) OP(uint32_t) OP(uint16_t) OP(uint8_t) OP(int64_t) OP(int32_t) OP(int16_t) OP(int8_t) OP(char) OP(float) OP(double)

        bool ReadNumber(Reader &reader, const Uuid &id, void *object)
        {
#define READ_NUMBER(T)                                                                                                                               \
    if (id == TypeInfo<T>::RegisteredId())                                                                                                           \
        return ReadScalar<T>(reader, object);
            WORLD_SCALARS(READ_NUMBER)
#undef READ_NUMBER
            return Fail(reader.error, "Unsupported World arithmetic type");
        }
        bool WriteNumber(Writer &writer, const Uuid &id, const void *object)
        {
#define WRITE_NUMBER(T)                                                                                                                              \
    if (id == TypeInfo<T>::RegisteredId())                                                                                                           \
        return WriteScalar(writer, *static_cast<const T *>(object));
            WORLD_SCALARS(WRITE_NUMBER)
#undef WRITE_NUMBER
            return Fail(writer.error, "Unsupported World arithmetic type");
        }
        bool JsonNumber(Writer &writer, const Uuid &id, const Json &json)
        {
#define JSON_NUMBER(T)                                                                                                                               \
    if (id == TypeInfo<T>::RegisteredId())                                                                                                           \
        return JsonScalar<T>(json, writer);
            WORLD_SCALARS(JSON_NUMBER)
#undef JSON_NUMBER
            return Fail(writer.error, "Unsupported World arithmetic type");
        }
#undef WORLD_SCALARS

        bool ParseUuid(const Json &json, Uuid &id, std::string &error)
        {
            if (!json.IsString() || json.GetStringLength() != 36)
                return Fail(error, "Invalid World UUID");
            const std::string_view text(json.GetString(), json.GetStringLength());
            for (size_t i = 0; i < text.size(); ++i)
                if ((i == 8 || i == 13 || i == 18 || i == 23) ? text[i] != '-' : GetDigit(text[i]) == std::string_view::npos)
                    return Fail(error, "Invalid World UUID");
            id = Uuid::CreateFromString(text);
            return true;
        }

        bool Keys(const Json &json, std::initializer_list<std::string_view> allowed, std::string &error)
        {
            if (!json.IsObject())
                return Fail(error, "Expected World object");
            std::set<std::string_view> names;
            for (auto member = json.MemberBegin(); member != json.MemberEnd(); ++member) {
                const std::string_view name(member->name.GetString(), member->name.GetStringLength());
                if (std::find(allowed.begin(), allowed.end(), name) == allowed.end() || !names.insert(name).second)
                    return Fail(error, "Unknown or duplicate World field: " + std::string(name));
            }
            return true;
        }

        bool ReadValue(Reader &, const TypeInfoRT *, void *, Budget &, uint32_t);
        bool CaptureValue(Writer &, const TypeInfoRT *, const void *, Budget &, uint32_t);
        bool JsonValue(Writer &, const TypeInfoRT *, const Json &, Budget &, uint32_t);

        serialize::ConstructFn DefaultAnyConstructor(const TypeInfoRT *type)
        {
            const auto *node = type ? GetTypeNode(type->registeredId) : nullptr;
            if (node)
                for (const auto &constructor : node->constructList)
                    if (!constructor.argsNum && constructor.constructFn)
                        return constructor.constructFn;
            return nullptr;
        }

        bool DefaultAny(const TypeInfoRT *type, Any &out, std::string &error)
        {
            const auto constructor = DefaultAnyConstructor(type);
            if (!constructor)
                return Fail(error, "World Any payload has no reflected default constructor");
            out = constructor(nullptr);
            return (out.Info() && out.Info()->registeredId == type->registeredId) || Fail(error, "World default constructor type mismatch");
        }

        bool ReadValue(Reader &reader, const TypeInfoRT *type, void *object, Budget &budget, uint32_t depth)
        {
            if (!budget.Enter(depth, reader.error))
                return false;
            uint64_t                 tag = 0, length = 0;
            Uuid                     id;
            std::span<const uint8_t> bytes;
            const auto               kind = ValueKind(type);
            if (kind == Kind::INVALID || !reader.Integer(tag, 1) || !reader.Id(id) || !reader.Integer(length, 4) ||
                tag != static_cast<uint8_t>(kind) || id != type->registeredId || !reader.Take(length, bytes))
                return Fail(reader.error, "World typed-value schema or length mismatch");
            Reader value(bytes, reader.error);
            switch (kind) {
            case Kind::SCALAR:
                if (!ReadNumber(value, id, object))
                    return false;
                break;
            case Kind::UUID: {
                Uuid uuid;
                if (!value.Id(uuid))
                    return false;
                if (object)
                    *static_cast<Uuid *>(object) = uuid;
                break;
            }
            case Kind::STRING:
            case Kind::NAME: {
                std::string_view text;
                if (!value.Text(text))
                    return false;
                if (kind == Kind::NAME && text.find('\0') != std::string_view::npos)
                    return Fail(reader.error, "World Name contains a null character");
                if (object) {
                    if (kind == Kind::STRING)
                        *static_cast<std::string *>(object) = text;
                    else
                        *static_cast<Name *>(object) = Name(std::string(text).c_str());
                }
                break;
            }
            case Kind::MATH: {
                std::array<float, 16> numbers{};
                const auto            count = MathCount(id);
                for (uint32_t i = 0; i < count; ++i)
                    if (!ReadScalar<float>(value, &numbers[i]))
                        return false;
                if (object)
                    std::memcpy(object, numbers.data(), count * sizeof(float));
                break;
            }
            case Kind::ENUM:
                if (!ReadValue(value, FindType(type->underlyingTypeId), object, budget, depth + 1))
                    return false;
                break;
            case Kind::SEQUENCE: {
                uint32_t count = 0;
                if (!value.Count(count, WorldAssetData::MAX_ELEMENTS, VALUE_HEADER))
                    return false;
                const auto     *elementType = FindType(type->containerInfo->valueType);
                SequenceVisitor visitor(type->containerInfo, object);
                if (object)
                    while (visitor.Count())
                        visitor.Erase(visitor.Count() - 1);
                for (uint32_t i = 0; i < count; ++i)
                    if (!ReadValue(value, elementType, object ? visitor.Emplace() : nullptr, budget, depth + 1))
                        return false;
                break;
            }
            case Kind::ANY: {
                uint64_t present = 0;
                if (!value.Integer(present, 1) || present > 1)
                    return Fail(reader.error, "Invalid World Any discriminator");
                Any candidate;
                if (present) {
                    Uuid dynamicId;
                    if (!value.Id(dynamicId))
                        return false;
                    const auto *dynamicType = FindType(dynamicId);
                    if (!dynamicType)
                        return Fail(reader.error, "Unknown World Any payload type");
                    if (!DefaultAnyConstructor(dynamicType))
                        return Fail(reader.error, "World Any payload has no reflected default constructor");
                    if (object && !DefaultAny(dynamicType, candidate, reader.error))
                        return false;
                    if (!ReadValue(value, dynamicType, object ? candidate.Data() : nullptr, budget, depth + 1))
                        return false;
                }
                if (object)
                    *static_cast<Any *>(object) = std::move(candidate);
                break;
            }
            case Kind::OBJECT: {
                const auto *node  = GetTypeNode(id);
                uint32_t    count = 0;
                if (!value.Count(count, WorldAssetData::MAX_ELEMENTS, 4 + VALUE_HEADER) || count != node->members.size())
                    return Fail(reader.error, "World object field count disagrees with its schema");
                for (const auto &[name, member] : node->members) {
                    std::string_view field;
                    if (!member.getterConstFn || !member.setterFn || !value.Text(field) || field != name)
                        return Fail(reader.error, "World field is unknown, duplicated or not writable");
                    Any        current;
                    const bool dynamic = member.info && member.info->registeredId == TypeInfo<Any>::RegisteredId();
                    if (object) {
                        current = member.getterConstFn(object);
                        if (!dynamic && !current.Data())
                            return Fail(reader.error, "World field getter failed");
                    }
                    void *storage = dynamic ? static_cast<void *>(&current) : current.Data();
                    if (!ReadValue(value, member.info, object ? storage : nullptr, budget, depth + 1))
                        return false;
                    if (object && !member.setterFn(object, storage))
                        return Fail(reader.error, "World field setter failed");
                }
                break;
            }
            default: return Fail(reader.error, "Unsupported World value kind");
            }
            return value.End();
        }

        bool CaptureValue(Writer &writer, const TypeInfoRT *type, const void *object, Budget &budget, uint32_t depth)
        {
            if (!object || !budget.Enter(depth, writer.error))
                return Fail(writer.error, "World persistence data is unavailable or exceeds its budget");
            Kind   kind;
            size_t lengthPosition = 0;
            if (!BeginValue(writer, type, kind, lengthPosition))
                return false;
            const auto &id = type->registeredId;
            switch (kind) {
            case Kind::SCALAR:
                if (!WriteNumber(writer, id, object))
                    return false;
                break;
            case Kind::UUID:
                if (!writer.Id(*static_cast<const Uuid *>(object)))
                    return false;
                break;
            case Kind::STRING:
                if (!writer.Text(*static_cast<const std::string *>(object)))
                    return false;
                break;
            case Kind::NAME:
                if (!writer.Text(static_cast<const Name *>(object)->GetStr()))
                    return false;
                break;
            case Kind::MATH: {
                std::array<float, 16> numbers{};
                const auto            count = MathCount(id);
                std::memcpy(numbers.data(), object, count * sizeof(float));
                for (uint32_t i = 0; i < count; ++i)
                    if (!WriteScalar(writer, numbers[i]))
                        return false;
                break;
            }
            case Kind::ENUM:
                if (!CaptureValue(writer, FindType(type->underlyingTypeId), object, budget, depth + 1))
                    return false;
                break;
            case Kind::SEQUENCE: {
                SequenceVisitor visitor(type->containerInfo, const_cast<void *>(object));
                const auto      count = visitor.Count();
                if (count > WorldAssetData::MAX_ELEMENTS || !writer.Integer(count, 4))
                    return Fail(writer.error, "World sequence is too large");
                const auto *elementType = FindType(type->containerInfo->valueType);
                for (size_t i = 0; i < count; ++i)
                    if (!CaptureValue(writer, elementType, visitor.GetByIndex(i), budget, depth + 1))
                        return false;
                break;
            }
            case Kind::ANY: {
                const auto &any = *static_cast<const Any *>(object);
                if (!writer.Integer(any ? 1 : 0, 1))
                    return false;
                if (any && (!writer.Id(any.Info()->registeredId) || !CaptureValue(writer, any.Info(), any.Data(), budget, depth + 1)))
                    return false;
                break;
            }
            case Kind::OBJECT: {
                const auto *node = GetTypeNode(id);
                if (!writer.Integer(node->members.size(), 4))
                    return false;
                for (const auto &[name, member] : node->members) {
                    if (!member.getterConstFn || !member.setterFn)
                        return Fail(writer.error, "World persistence field is not readable and writable");
                    const auto  value = member.getterConstFn(object);
                    const void *storage =
                        member.info && member.info->registeredId == TypeInfo<Any>::RegisteredId() ? static_cast<const void *>(&value) : value.Data();
                    if (!writer.Text(name) || !CaptureValue(writer, member.info, storage, budget, depth + 1))
                        return false;
                }
                break;
            }
            default: return Fail(writer.error, "Unsupported World value kind");
            }
            writer.PatchLength(lengthPosition, writer.bytes.size() - lengthPosition - 4);
            return true;
        }

        bool JsonValue(Writer &writer, const TypeInfoRT *type, const Json &json, Budget &budget, uint32_t depth)
        {
            if (!budget.Enter(depth, writer.error))
                return false;
            Kind   kind;
            size_t lengthPosition = 0;
            if (!BeginValue(writer, type, kind, lengthPosition))
                return false;
            const auto &id = type->registeredId;
            switch (kind) {
            case Kind::SCALAR:
                if (!JsonNumber(writer, id, json))
                    return false;
                break;
            case Kind::UUID: {
                Uuid uuid;
                if (!ParseUuid(json, uuid, writer.error) || !writer.Id(uuid))
                    return false;
                break;
            }
            case Kind::STRING:
            case Kind::NAME:
                if (!json.IsString() || (kind == Kind::NAME && std::memchr(json.GetString(), 0, json.GetStringLength())) ||
                    !writer.Text({json.GetString(), json.GetStringLength()}))
                    return Fail(writer.error, "Expected World text value");
                break;
            case Kind::MATH: {
                const auto count = MathCount(id);
                if (!json.IsArray() || json.Size() != count)
                    return Fail(writer.error, "World math array has the wrong size");
                for (const auto &number : json.GetArray())
                    if (!JsonScalar<float>(number, writer))
                        return false;
                break;
            }
            case Kind::ENUM:
                if (!JsonValue(writer, FindType(type->underlyingTypeId), json, budget, depth + 1))
                    return false;
                break;
            case Kind::SEQUENCE:
                if (!json.IsArray() || json.Size() > WorldAssetData::MAX_ELEMENTS || !writer.Integer(json.Size(), 4))
                    return Fail(writer.error, "Invalid World sequence");
                for (const auto &element : json.GetArray())
                    if (!JsonValue(writer, FindType(type->containerInfo->valueType), element, budget, depth + 1))
                        return false;
                break;
            case Kind::ANY: {
                if (json.IsNull()) {
                    if (!writer.Integer(0, 1))
                        return false;
                } else {
                    if (!json.IsObject() || !json.HasMember("classId"))
                        return Fail(writer.error, "World Any requires a reflected classId");
                    Uuid dynamicId;
                    if (!ParseUuid(json["classId"], dynamicId, writer.error) || !writer.Integer(1, 1) || !writer.Id(dynamicId) ||
                        !JsonValue(writer, FindType(dynamicId), json, budget, depth + 1))
                        return false;
                }
                break;
            }
            case Kind::OBJECT: {
                if (!Keys(json, {"classId", "elements"}, writer.error) || !json.HasMember("classId") || !json.HasMember("elements"))
                    return Fail(writer.error, "World reflected object requires classId and elements");
                Uuid classId;
                if (!ParseUuid(json["classId"], classId, writer.error) || classId != id)
                    return Fail(writer.error, "World reflected classId disagrees with its schema");
                const auto &elements = json["elements"];
                const auto *node     = GetTypeNode(id);
                if (!elements.IsObject() || elements.MemberCount() != node->members.size() || !writer.Integer(node->members.size(), 4))
                    return Fail(writer.error, "World object fields disagree with its schema");
                std::set<std::string_view> seen;
                for (auto member = elements.MemberBegin(); member != elements.MemberEnd(); ++member) {
                    const std::string_view name(member->name.GetString(), member->name.GetStringLength());
                    if (!node->members.contains(name) || !seen.insert(name).second)
                        return Fail(writer.error, "Unknown or duplicate World reflected field");
                }
                for (const auto &[name, member] : node->members) {
                    if (!member.getterConstFn || !member.setterFn)
                        return Fail(writer.error, "World persistence field is not readable and writable");
                    if (!writer.Text(name) || !JsonValue(writer, member.info, elements[std::string(name).c_str()], budget, depth + 1))
                        return false;
                }
                break;
            }
            default: return Fail(writer.error, "Unsupported World value kind");
            }
            writer.PatchLength(lengthPosition, writer.bytes.size() - lengthPosition - 4);
            return true;
        }

        bool ComponentType(const Uuid &id)
        {
            for (const auto &[group, types] : ComponentFactory::Get()->GetTypes())
                if (std::any_of(types.begin(), types.end(), [&](const auto &entry) { return entry.typeId == id; }))
                    return true;
            return false;
        }

        std::unique_ptr<ComponentBase> CreateComponent(const Uuid &id, std::string &error)
        {
            const auto *type = FindType(id);
            if (!ComponentType(id) || !type || !type->newFunc) {
                Fail(error, "Unknown or non-constructible World component: " + id.ToString());
                return {};
            }
            auto component = ComponentFactory::Get()->Create(id);
            if (!component || component->GetTypeId() != id || !component->GetPersistentDataType() || !component->GetPersistentData()) {
                Fail(error, "World component has no typed persistence data");
                return {};
            }
            return component;
        }

        bool Payload(const Uuid &id, std::span<const uint8_t> bytes, void *object, Budget &budget, std::string &error)
        {
            Reader reader(bytes, error);
            return ReadValue(reader, FindType(id), object, budget, 0) && reader.End();
        }

        bool Hierarchy(const WorldAssetData &data, std::string &error)
        {
            std::unordered_map<Uuid, size_t> indices;
            for (size_t i = 0; i < data.actors.size(); ++i)
                if (!data.actors[i].uuid || !indices.emplace(data.actors[i].uuid, i).second)
                    return Fail(error, "World actors require unique non-empty identities");
            const auto          count = data.actors.size();
            std::vector<size_t> parents(count, count);
            std::vector<bool>   transforms(count, false);
            for (size_t i = 0; i < count; ++i)
                for (const auto &component : data.actors[i].components)
                    if (component.componentType == TypeInfo<TransformComponent>::RegisteredId()) {
                        if (component.dataType != TypeInfo<TransformData>::RegisteredId())
                            return Fail(error, "World Transform persistence type mismatch");
                        TransformData transform{};
                        Budget        budget;
                        if (!Payload(component.dataType, component.payload, &transform, budget, error))
                            return false;
                        transforms[i] = true;
                        if (transform.parent) {
                            const auto parent = indices.find(transform.parent);
                            if (parent == indices.end() || parent->second == i)
                                return Fail(error, "World Transform parent is missing or references itself");
                            parents[i] = parent->second;
                        }
                    }
            for (size_t i = 0; i < count; ++i)
                if (parents[i] != count && !transforms[parents[i]])
                    return Fail(error, "World Transform parent has no Transform component");
            std::vector<uint8_t> colors(count, 0);
            for (size_t i = 0; i < count; ++i) {
                size_t current = i;
                while (current != count && colors[current] == 0) {
                    colors[current] = 1;
                    current         = parents[current];
                }
                if (current != count && colors[current] == 1)
                    return Fail(error, "World Transform hierarchy contains a cycle");
                current = i;
                while (current != count && colors[current] == 1) {
                    colors[current] = 2;
                    current         = parents[current];
                }
            }
            return true;
        }

        bool Config(const WorldSubSystemAsset &entry, Any &config, std::string &error)
        {
            const auto *registration = WorldSubSystemRegistry::Get().GetRegistration(Name(entry.name.c_str()));
            if (!registration || !registration->factory)
                return Fail(error, "Unknown World subsystem: " + entry.name);
            if (registration->configType) {
                if (entry.configType != registration->configType->registeredId || !registration->makeDefaultConfig)
                    return Fail(error, "World subsystem config type or default factory is unavailable");
                config = registration->makeDefaultConfig();
                if (!config.Info() || config.Info()->registeredId != entry.configType)
                    return Fail(error, "World subsystem config factory returned the wrong type");
                Budget budget;
                if (!Payload(entry.configType, entry.payload, config.Data(), budget, error))
                    return false;
            } else if (entry.configType || !entry.payload.empty()) {
                return Fail(error, "World subsystem does not accept a config");
            }
            return !registration->validate || registration->validate(config, error) ||
                   Fail(error, error.empty() ? "World subsystem config validation failed" : error);
        }

        bool WriteBody(const WorldAssetData &data, Writer &writer)
        {
            if (!writer.Integer(data.roots.size(), 4))
                return false;
            for (const auto &root : data.roots)
                if (!writer.Id(root.uuid) || !writer.Text(root.type))
                    return false;
            if (!writer.Integer(data.actors.size(), 4))
                return false;
            for (const auto &actor : data.actors) {
                if (!writer.Id(actor.uuid) || !writer.Text(actor.name) || !writer.Integer(actor.components.size(), 4))
                    return false;
                for (const auto &component : actor.components)
                    if (!writer.Id(component.componentType) || !writer.Id(component.dataType) || !writer.Blob(component.payload))
                        return false;
            }
            if (!writer.Integer(data.subSystems.size(), 4))
                return false;
            for (const auto &entry : data.subSystems)
                if (!writer.Text(entry.name) || !writer.Integer(entry.enabled ? 1 : 0, 1) || !writer.Id(entry.configType) ||
                    !writer.Blob(entry.payload))
                    return false;
            return true;
        }

        bool ReadBody(Reader &reader, WorldAssetData &data)
        {
            uint32_t count   = 0;
            uint64_t records = 0;
            if (!reader.Count(count, WorldAssetData::MAX_ROOTS, 20))
                return false;
            data.roots.resize(count);
            for (auto &root : data.roots)
                if (!reader.Id(root.uuid) || !reader.Text(root.type))
                    return false;
            if (!reader.Count(count, WorldAssetData::MAX_ACTORS, 24))
                return false;
            records += count;
            data.actors.resize(count);
            for (auto &actor : data.actors) {
                uint32_t components = 0;
                if (!reader.Id(actor.uuid) || !reader.Text(actor.name) || !reader.Count(components, WorldAssetData::MAX_COMPONENTS, 36))
                    return false;
                records += components;
                if (records > WorldAssetData::MAX_ELEMENTS)
                    return Fail(reader.error, "World record allocation budget exceeded");
                actor.components.resize(components);
                for (auto &component : actor.components)
                    if (!reader.Id(component.componentType) || !reader.Id(component.dataType) || !reader.Blob(component.payload))
                        return false;
            }
            if (!reader.Count(count, WorldAssetData::MAX_SUBSYSTEMS, 25))
                return false;
            data.subSystems.resize(count);
            for (auto &entry : data.subSystems) {
                uint64_t enabled = 0;
                if (!reader.Text(entry.name) || !reader.Integer(enabled, 1) || enabled > 1 || !reader.Id(entry.configType) ||
                    !reader.Blob(entry.payload))
                    return Fail(reader.error, "Invalid World subsystem record");
                entry.enabled = enabled != 0;
            }
            return reader.End();
        }

        bool Remaining(IStreamArchive &input, size_t &size, std::string &error)
        {
            auto      &stream = input.GetStream();
            const auto start  = stream.tellg();
            if (start < 0)
                return Fail(error, "World stream position is unavailable");
            stream.seekg(0, std::ios::end);
            const auto end = stream.tellg();
            stream.seekg(start);
            if (!stream.good() || end < start || static_cast<uint64_t>(end - start) > WorldAssetData::MAX_BYTES)
                return Fail(error, "World stream is invalid or exceeds its byte budget");
            size = static_cast<size_t>(end - start);
            return true;
        }

        bool JsonText(const Json &json, std::string &out, std::string &error)
        {
            if (!json.IsString() || json.GetStringLength() > WorldAssetData::MAX_STRING || std::memchr(json.GetString(), 0, json.GetStringLength()))
                return Fail(error, "Invalid World text field");
            out.assign(json.GetString(), json.GetStringLength());
            return true;
        }

        bool Required(const Json &json, std::initializer_list<std::string_view> fields, std::string &error)
        {
            for (auto field : fields)
                if (!json.HasMember(std::string(field).c_str()))
                    return Fail(error, "Missing World field: " + std::string(field));
            return true;
        }

        bool JsonBudget(std::string_view text, std::string &error)
        {
            uint32_t depth = 0, tokens = 0;
            bool     quoted = false, escaped = false;
            for (const auto ch : text) {
                if (quoted) {
                    if (escaped)
                        escaped = false;
                    else if (ch == '\\')
                        escaped = true;
                    else if (ch == '"')
                        quoted = false;
                } else if (ch == '"')
                    quoted = true;
                else if (ch == '[' || ch == '{') {
                    if (++depth > WorldAssetData::MAX_DEPTH * 2 + 16 || ++tokens > WorldAssetData::MAX_ELEMENTS)
                        return Fail(error, "World JSON nesting or element budget exceeded");
                } else if (ch == ']' || ch == '}') {
                    if (!depth)
                        return Fail(error, "Unbalanced World JSON document");
                    --depth;
                } else if (ch == ',' && ++tokens > WorldAssetData::MAX_ELEMENTS)
                    return Fail(error, "World JSON element budget exceeded");
            }
            return (!quoted && !depth) || Fail(error, "Incomplete World JSON document");
        }
    } // namespace

    bool WorldAssetData::Validate(std::string &error) const
    {
        error.clear();
        if (!loadError.empty())
            return Fail(error, loadError);
        if (actors.size() > MAX_ACTORS || subSystems.size() > MAX_SUBSYSTEMS || roots.size() > MAX_ROOTS)
            return Fail(error, "World record count exceeds its budget");
        uint64_t   bytes   = 12;
        uint64_t   records = actors.size();
        const auto add     = [&](uint64_t count) {
            bytes += count;
            return bytes <= MAX_BYTES;
        };
        std::map<Uuid, std::string> rootTypes;
        for (const auto &root : roots) {
            if (!root.uuid || root.type.empty() || root.type.size() > MAX_STRING || !Utf8(root.type) || root.type.find('\0') != std::string::npos ||
                !add(20 + root.type.size()))
                return Fail(error, "Invalid World asset root or byte budget");
            const auto [it, inserted] = rootTypes.emplace(root.uuid, root.type);
            if (!inserted && it->second != root.type)
                return Fail(error, "World asset root has conflicting types");
        }
        Budget budget;
        for (const auto &actor : actors) {
            if (actor.name.size() > MAX_STRING || !Utf8(actor.name) || actor.name.find('\0') != std::string::npos ||
                actor.components.size() > MAX_COMPONENTS || !add(24 + actor.name.size()))
                return Fail(error, "Invalid World actor or byte budget");
            records += actor.components.size();
            if (records > MAX_ELEMENTS)
                return Fail(error, "World component count exceeds its budget");
            std::set<Uuid> components;
            for (const auto &component : actor.components) {
                if (!ComponentType(component.componentType) || !components.insert(component.componentType).second || !component.dataType ||
                    !add(36 + component.payload.size()))
                    return Fail(error, "Unknown, duplicate or oversized World component");
                const auto *type = ComponentFactory::Get()->GetPersistentDataType(component.componentType);
                if (!type)
                    return Fail(error, "World component has no static persistence schema");
                if (type->registeredId != component.dataType)
                    return Fail(error, "World component data schema mismatch");
                if (!Payload(component.dataType, component.payload, nullptr, budget, error))
                    return false;
            }
        }
        std::set<std::string> systems;
        for (const auto &entry : subSystems) {
            if (entry.name.empty() || entry.name.size() > MAX_STRING || !Utf8(entry.name) || entry.name.find('\0') != std::string::npos ||
                !systems.insert(entry.name).second || !add(25 + entry.name.size() + entry.payload.size()))
                return Fail(error, "Invalid, duplicate or oversized World subsystem");
            const auto *registration = WorldSubSystemRegistry::Get().GetRegistration(Name(entry.name.c_str()));
            if (!registration || !registration->factory ||
                (registration->configType ? registration->configType->registeredId : Uuid{}) != entry.configType)
                return Fail(error, "Unknown World subsystem or config schema mismatch");
            if (entry.configType) {
                if (!Payload(entry.configType, entry.payload, nullptr, budget, error))
                    return false;
            } else if (!entry.payload.empty())
                return Fail(error, "World subsystem without a config has a payload");
            Any config;
            if (!Config(entry, config, error))
                return false;
        }
        return Hierarchy(*this, error);
    }

    void WorldAssetData::Save(BinaryOutputArchive &archive) const
    {
        std::string error;
        if (!Validate(error)) {
            archive.Invalidate();
            return;
        }
        Writer body(error);
        if (!WriteBody(*this, body)) {
            archive.Invalidate();
            return;
        }
        Writer header(error);
        if (!header.Integer(MAGIC, 4) || !header.Integer(CURRENT_VERSION, 4) || !header.Integer(body.bytes.size(), 4)) {
            archive.Invalidate();
            return;
        }
        archive.SaveValue(reinterpret_cast<const char *>(header.bytes.data()), header.bytes.size());
        archive.SaveValue(reinterpret_cast<const char *>(body.bytes.data()), body.bytes.size());
    }

    void WorldAssetData::Load(BinaryInputArchive &archive)
    {
        std::string             error;
        WorldAssetData          candidate;
        std::array<uint8_t, 12> header{};
        archive.LoadValue(reinterpret_cast<char *>(header.data()), header.size());
        Reader   reader(header, error);
        uint64_t magic = 0, version = 0, length = 0;
        size_t   remaining = 0;
        bool     valid = archive.IsValid() && reader.Integer(magic, 4) && reader.Integer(version, 4) && reader.Integer(length, 4) && magic == MAGIC &&
                     version == CURRENT_VERSION && length <= MAX_BYTES &&
                     Remaining(const_cast<IStreamArchive &>(archive.GetStream()), remaining, error) && length == remaining;
        if (valid) {
            Bytes bytes(static_cast<size_t>(length));
            archive.LoadValue(reinterpret_cast<char *>(bytes.data()), bytes.size());
            Reader body(bytes, error);
            valid = archive.IsValid() && ReadBody(body, candidate) && candidate.Validate(error);
        }
        if (valid)
            *this = std::move(candidate);
        else {
            actors.clear();
            subSystems.clear();
            roots.clear();
            loadError = error.empty() ? "Invalid World binary header, version or length" : std::move(error);
            archive.Invalidate();
        }
    }

    bool CookWorldJson(IStreamArchive &source, WorldAssetData &out, std::string &error)
    {
        error.clear();
        size_t size = 0;
        if (!Remaining(source, size, error))
            return false;
        std::string text(size, '\0');
        if (!source.LoadRaw(text.data(), text.size()))
            return Fail(error, "World source could not be read completely");
        if (!JsonBudget(text, error))
            return false;
        rapidjson::Document document;
        document.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(text.data(), text.size());
        if (document.HasParseError() || !Keys(document, {"actors", "subSystems", "assetRoots"}, error) ||
            !Required(document, {"actors", "subSystems", "assetRoots"}, error))
            return Fail(error, "Invalid World source document");
        if (!document["actors"].IsArray() || document["actors"].Size() > WorldAssetData::MAX_ACTORS || !document["subSystems"].IsArray() ||
            document["subSystems"].Size() > WorldAssetData::MAX_SUBSYSTEMS || !document["assetRoots"].IsArray() ||
            document["assetRoots"].Size() > WorldAssetData::MAX_ROOTS)
            return Fail(error, "Invalid World source record arrays");
        WorldAssetData candidate;
        for (const auto &root : document["assetRoots"].GetArray()) {
            ComponentAssetReference reference;
            if (!Keys(root, {"uuid", "type"}, error) || !Required(root, {"uuid", "type"}, error) || !ParseUuid(root["uuid"], reference.uuid, error) ||
                !JsonText(root["type"], reference.type, error))
                return false;
            candidate.roots.push_back(std::move(reference));
        }
        Budget budget;
        for (const auto &actor : document["actors"].GetArray()) {
            if (!Keys(actor, {"uuid", "name", "components"}, error) || !Required(actor, {"uuid", "name", "components"}, error))
                return false;
            WorldActorAsset entry;
            if (!ParseUuid(actor["uuid"], entry.uuid, error) || !JsonText(actor["name"], entry.name, error) || !actor["components"].IsArray() ||
                actor["components"].Size() > WorldAssetData::MAX_COMPONENTS)
                return Fail(error, "Invalid World actor record");
            for (const auto &component : actor["components"].GetArray()) {
                if (!Keys(component, {"type", "data"}, error) || !Required(component, {"type", "data"}, error))
                    return false;
                WorldComponentAsset data;
                if (!ParseUuid(component["type"], data.componentType, error))
                    return false;
                const auto *type = ComponentFactory::Get()->GetPersistentDataType(data.componentType);
                if (!type)
                    return Fail(error, "Unknown World component or missing static persistence schema: " + data.componentType.ToString());
                data.dataType = type->registeredId;
                Writer writer(error);
                if (!JsonValue(writer, type, component["data"], budget, 0))
                    return false;
                data.payload = std::move(writer.bytes);
                entry.components.push_back(std::move(data));
            }
            candidate.actors.push_back(std::move(entry));
        }
        for (const auto &system : document["subSystems"].GetArray()) {
            if (!Keys(system, {"name", "enabled", "config"}, error) || !Required(system, {"name", "enabled"}, error))
                return false;
            WorldSubSystemAsset entry;
            if (!JsonText(system["name"], entry.name, error) || !system["enabled"].IsBool())
                return Fail(error, "Invalid World subsystem record");
            entry.enabled            = system["enabled"].GetBool();
            const auto *registration = WorldSubSystemRegistry::Get().GetRegistration(Name(entry.name.c_str()));
            if (!registration)
                return Fail(error, "Unknown World subsystem: " + entry.name);
            if (registration->configType) {
                entry.configType = registration->configType->registeredId;
                Writer writer(error);
                if (system.HasMember("config")) {
                    if (!JsonValue(writer, registration->configType, system["config"], budget, 0))
                        return false;
                } else {
                    Any config = registration->makeDefaultConfig ? registration->makeDefaultConfig() : Any{};
                    if (!config.Info() || config.Info()->registeredId != entry.configType ||
                        !CaptureValue(writer, config.Info(), config.Data(), budget, 0))
                        return Fail(error, "World subsystem requires a valid config or default factory");
                }
                entry.payload = std::move(writer.bytes);
            } else if (system.HasMember("config") && !system["config"].IsNull())
                return Fail(error, "World subsystem does not accept a config");
            Any checked;
            if (!Config(entry, checked, error))
                return false;
            candidate.subSystems.push_back(std::move(entry));
        }
        if (!candidate.Validate(error))
            return false;
        out = std::move(candidate);
        return true;
    }

    bool CaptureWorldAsset(const World &world, WorldAssetData &out, std::string &error)
    {
        error.clear();
        WorldAssetData candidate;
        Budget         budget;
        if (world.GetActors().size() > WorldAssetData::MAX_ACTORS)
            return Fail(error, "World actor count exceeds its budget");
        for (const auto &actor : world.GetActors()) {
            WorldActorAsset entry{actor->GetUuid(), actor->GetName(), {}};
            if (actor->GetComponents().size() > WorldAssetData::MAX_COMPONENTS)
                return Fail(error, "World component count exceeds its budget");
            for (const auto &[id, component] : actor->GetComponents()) {
                const auto *type = component->GetPersistentDataType();
                if (!type)
                    return Fail(error, "World component does not provide persistence data");
                WorldComponentAsset data{id, type->registeredId, {}};
                Writer              writer(error);
                if (!CaptureValue(writer, type, component->GetPersistentData(), budget, 0))
                    return false;
                data.payload = std::move(writer.bytes);
                entry.components.push_back(std::move(data));
                component->CollectAssetReferences(candidate.roots);
            }
            candidate.actors.push_back(std::move(entry));
        }
        if (const auto *description = world.GetWorldDesc()) {
            if (description->subSystems.size() > WorldAssetData::MAX_SUBSYSTEMS)
                return Fail(error, "World subsystem count exceeds its budget");
            for (const auto &system : description->subSystems) {
                WorldSubSystemAsset entry{std::string(system.name.GetStr()), system.enabled, {}, {}};
                const auto         *registration = WorldSubSystemRegistry::Get().GetRegistration(system.name);
                if (!registration)
                    return Fail(error, "Unknown World subsystem: " + entry.name);
                if (registration->configType) {
                    Any config = system.config ? system.config : registration->makeDefaultConfig ? registration->makeDefaultConfig() : Any{};
                    if (!config.Info() || config.Info()->registeredId != registration->configType->registeredId)
                        return Fail(error, "World subsystem config type mismatch");
                    entry.configType = config.Info()->registeredId;
                    Writer writer(error);
                    if (!CaptureValue(writer, config.Info(), config.Data(), budget, 0))
                        return false;
                    entry.payload = std::move(writer.bytes);
                } else if (system.config)
                    return Fail(error, "World subsystem does not accept a config");
                candidate.subSystems.push_back(std::move(entry));
            }
        }
        if (!candidate.Validate(error))
            return false;
        out = std::move(candidate);
        return true;
    }

    bool InstantiateWorldAsset(const WorldAssetData &data, WorldPtr &out, std::string &error)
    {
        error.clear();
        if (!data.Validate(error))
            return false;
        std::vector<std::unique_ptr<Actor>> actors;
        Budget                              budget;
        for (const auto &entry : data.actors) {
            auto actor = std::make_unique<Actor>(entry.uuid);
            actor->SetName(entry.name);
            for (const auto &saved : entry.components) {
                auto component = CreateComponent(saved.componentType, error);
                if (!component || component->GetPersistentDataType()->registeredId != saved.dataType)
                    return Fail(error, "World component data schema mismatch");
                if (!Payload(saved.dataType, saved.payload, component->GetPersistentData(), budget, error))
                    return false;
                auto *attached = component.get();
                if (!actor->AttachComponent(component))
                    return Fail(error, "World component attachment failed");
                attached->OnSerialized();
            }
            actors.push_back(std::move(actor));
        }
        WorldDesc description;
        for (const auto &entry : data.subSystems) {
            WorldSubSystemDesc system;
            system.name    = Name(entry.name.c_str());
            system.enabled = entry.enabled;
            if (!Config(entry, system.config, error))
                return false;
            description.subSystems.push_back(std::move(system));
        }
        WorldPtr candidate = World::CreateWorld();
        candidate->Init();
        std::unordered_map<Uuid, Actor *> index;
        for (const auto &actor : actors)
            index.emplace(actor->GetUuid(), actor.get());
        std::set<Uuid> linked;
        for (const auto &actor : actors) {
            std::vector<Actor *> chain;
            Actor               *current = actor.get();
            while (current && !linked.contains(current->GetUuid())) {
                chain.push_back(current);
                const auto *transform = current->GetComponent<TransformComponent>();
                const auto  parent    = transform ? index.find(transform->GetData().parent) : index.end();
                current               = parent != index.end() ? parent->second : nullptr;
            }
            for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
                if (auto *transform = (*it)->GetComponent<TransformComponent>(); transform && transform->GetData().parent) {
                    const auto parent = index.find(transform->GetData().parent);
                    if (parent == index.end())
                        return Fail(error, "World Transform parent disappeared during instantiation");
                    transform->SetParentPreserveLocal(parent->second->GetComponent<TransformComponent>());
                }
                linked.insert((*it)->GetUuid());
            }
        }
        candidate->actors = std::move(actors);
        for (size_t i = 0; i < candidate->actors.size(); ++i)
            candidate->actorIndex.emplace(candidate->actors[i]->GetUuid(), i);
        if (!candidate->Build(description))
            return Fail(error, candidate->GetLastError());
        out = std::move(candidate);
        return true;
    }
} // namespace sky
