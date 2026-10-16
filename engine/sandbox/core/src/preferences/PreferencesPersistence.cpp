#include <editor/core/preferences/PreferencesPersistence.h>
#include <editor/core/property/PropertyEditor.h>

#include "PreferenceGuard.h"

#include <core/type/TypeInfo.h>
#include <framework/serialization/ArrayVisitor.h>
#include <framework/serialization/SerializationContext.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace sky::editor {
    namespace {
        using Json                  = rapidjson::Value;
        using Allocator             = rapidjson::Document::AllocatorType;
        constexpr unsigned MaxDepth = 64;

        PreferenceResult Error(PreferenceCode code, std::string message, const std::string &page = {}, const std::string &path = {})
        {
            PreferenceResult result;
            result.code    = code;
            result.pageId  = page;
            result.path    = path;
            result.message = std::move(message);
            return result;
        }

        struct Codec {
            std::string           page;
            std::function<bool()> isCurrent;

            PreferenceResult Check(const std::string &path) const
            {
                if (detail::CheckPreferenceGuard(isCurrent))
                    return {};
                return Error(PreferenceCode::Conflict, "Preferences changed during the operation", page, path);
            }
            PreferenceResult Invalid(const std::string &path, const char *message) const
            {
                return Error(PreferenceCode::InvalidData, message, page, path);
            }
            PreferenceResult Unsupported(const std::string &path, const char *message) const
            {
                return Error(PreferenceCode::UnsupportedType, message, page, path);
            }
        };

        template <typename Callback>
        PreferenceResult ReflectionCall(const Codec &codec, const std::string &path, Callback &&callback)
        {
            auto result = codec.Check(path);
            if (!result.Succeeded())
                return result;
            // Only reflection/provider calls belong here. JSON processing and
            // ordinary container allocation remain outside this boundary.
            try {
                callback();
            }
            catch (const std::exception &error) {
                result = codec.Check(path);
                return result.Succeeded() ? Error(PreferenceCode::InvalidData, error.what(), codec.page, path) : result;
            }
            catch (...) {
                result = codec.Check(path);
                return result.Succeeded() ? codec.Invalid(path, "Could not access the reflected preferences candidate") : result;
            }
            return codec.Check(path);
        }

        std::string ChildPath(const std::string &path, std::string_view member)
        {
            return path.empty() ? std::string(member) : path + "." + std::string(member);
        }

        auto Find(Json &object, std::string_view name)
        {
            return object.FindMember(rapidjson::StringRef(name.data(), static_cast<rapidjson::SizeType>(name.size())));
        }

        auto Find(const Json &object, std::string_view name)
        {
            return object.FindMember(rapidjson::StringRef(name.data(), static_cast<rapidjson::SizeType>(name.size())));
        }

        void Add(Json &object, std::string_view name, Json &value, Allocator &allocator)
        {
            Json key;
            key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), allocator);
            object.AddMember(key, value, allocator);
        }

        template <typename T>
        struct Tag {
            using Type = T;
        };

        template <typename Visitor>
        bool NumberType(const TypeInfoRT *info, Visitor &&visitor)
        {
#define PREFERENCE_NUMBER(T)                                                                                                                         \
    if (info->registeredId == TypeInfo<T>::RegisteredId()) {                                                                                         \
        visitor(Tag<T>{});                                                                                                                           \
        return true;                                                                                                                                 \
    }
            PREFERENCE_NUMBER(int8_t)
            PREFERENCE_NUMBER(uint8_t)
            PREFERENCE_NUMBER(int16_t)
            PREFERENCE_NUMBER(uint16_t)
            PREFERENCE_NUMBER(int32_t)
            PREFERENCE_NUMBER(uint32_t)
            PREFERENCE_NUMBER(int64_t)
            PREFERENCE_NUMBER(uint64_t)
            PREFERENCE_NUMBER(char)
            PREFERENCE_NUMBER(float)
            PREFERENCE_NUMBER(double)
#undef PREFERENCE_NUMBER
            return false;
        }

        bool Scalar(const TypeInfoRT *info)
        {
            return info->registeredId == TypeInfo<bool>::RegisteredId() || info->registeredId == TypeInfo<std::string>::RegisteredId() ||
                   NumberType(info, [](auto) {});
        }

        const TypeInfoRT *ElementType(const TypeInfoRT *info)
        {
            const auto *node = info->containerInfo != nullptr ? GetTypeNode(info->containerInfo->valueType) : nullptr;
            return node != nullptr ? node->info : nullptr;
        }

        const TypeInfoRT *UnderlyingType(const TypeInfoRT *info)
        {
            const auto *node = GetTypeNode(info->underlyingTypeId);
            return node != nullptr ? node->info : nullptr;
        }

        const TypeNode *Node(const TypeInfoRT *info, const TypeNode *hint = nullptr)
        {
            return hint != nullptr && hint->info == info ? hint : GetTypeNode(info);
        }

        PreferenceResult CheckType(const Codec                            &codec,
                                   const TypeInfoRT                       *info,
                                   const TypeNode                         *hint,
                                   const std::string                      &path,
                                   std::unordered_set<const TypeInfoRT *> &checked)
        {
            if (info == nullptr || info->staticInfo == nullptr || info->copy == nullptr) {
                return codec.Unsupported(path, "Settings values must have reflection and copy construction");
            }
            if (!checked.insert(info).second || Scalar(info)) {
                return {};
            }
            if (info->staticInfo->isPointer || info->staticInfo->isMemberObjectPointer || info->staticInfo->isMemberFunctionPointer) {
                return codec.Unsupported(path, "Pointer settings are not supported");
            }
            const auto *node = Node(info, hint);
            if (node != nullptr && (node->serialization.jsonLoad != nullptr || node->serialization.jsonSave != nullptr)) {
                return codec.Unsupported(path, "Custom JSON serializers need a result-aware settings codec");
            }
            if (info->staticInfo->isEnum) {
                const auto *underlying = UnderlyingType(info);
                bool        integral   = false;
                if (underlying != nullptr) {
                    NumberType(underlying, [&integral](auto tag) { integral = std::is_integral_v<typename decltype(tag)::Type>; });
                }
                return node != nullptr && integral && underlying->staticInfo->size == info->staticInfo->size
                           ? PreferenceResult{}
                           : codec.Unsupported(path, "Enum storage is not a registered integer type");
            }
            if (info->containerInfo != nullptr && info->containerInfo->sequenceView != nullptr) {
                const auto *element = ElementType(info);
                if (element == nullptr || element->newFunc == nullptr) {
                    return codec.Unsupported(path, "Sequence elements need registered default-constructible types");
                }
                return CheckType(codec, element, nullptr, path + "[]", checked);
            }
            if (node == nullptr || !info->registeredId || !info->staticInfo->isClass || info->staticInfo->isSequenceContainer ||
                info->staticInfo->isAssociativeContainer || node->base != nullptr || node->members.empty()) {
                return codec.Unsupported(path, "Only native scalars, enums, reflected structs and sequences are supported");
            }
            for (const auto &[name, member] : node->members) {
                const auto memberPath = ChildPath(path, name);
                if (member.getterConstFn == nullptr && member.getterFn == nullptr) {
                    return codec.Unsupported(memberPath, "Settings members must be readable");
                }
                auto result = CheckType(codec, member.info, nullptr, memberPath, checked);
                if (!result.Succeeded()) {
                    return result;
                }
            }
            return {};
        }

        template <typename T>
        bool Fits(const Json &json)
        {
            if constexpr (std::is_floating_point_v<T>) {
                return json.IsNumber() && std::isfinite(json.GetDouble()) && json.GetDouble() >= -std::numeric_limits<T>::max() &&
                       json.GetDouble() <= std::numeric_limits<T>::max();
            } else if constexpr (std::is_signed_v<T>) {
                return json.IsInt64() && json.GetInt64() >= std::numeric_limits<T>::min() && json.GetInt64() <= std::numeric_limits<T>::max();
            } else {
                return json.IsUint64() && json.GetUint64() <= std::numeric_limits<T>::max();
            }
        }

        PreferenceResult CheckRange(const Codec &codec, const Json &json, const serialize::TypeMemberNode &member, const std::string &path)
        {
            const auto attributes = ReadPropertyAttributes(member);
            if (attributes.hasRange && json.IsNumber()) {
                const auto value = json.GetDouble();
                if (!std::isfinite(attributes.rangeMin) || !std::isfinite(attributes.rangeMax) || attributes.rangeMin > attributes.rangeMax ||
                    value < attributes.rangeMin || value > attributes.rangeMax) {
                    return codec.Invalid(path, "Settings number is outside its reflected range");
                }
            }
            return {};
        }

        PreferenceResult
        CheckJson(const Codec &codec, const Json &json, const TypeInfoRT *info, const TypeNode *hint, const std::string &path, unsigned depth = 0)
        {
            if (depth > MaxDepth) {
                return codec.Invalid(path, "Settings nesting is too deep");
            }
            if (info->registeredId == TypeInfo<bool>::RegisteredId()) {
                return json.IsBool() ? PreferenceResult{} : codec.Invalid(path, "Expected a boolean");
            }
            if (info->registeredId == TypeInfo<std::string>::RegisteredId()) {
                return json.IsString() ? PreferenceResult{} : codec.Invalid(path, "Expected a string");
            }
            bool fits = false;
            if (NumberType(info, [&](auto tag) { fits = Fits<typename decltype(tag)::Type>(json); })) {
                return fits ? PreferenceResult{} : codec.Invalid(path, "Expected a representable number of the declared type");
            }
            if (info->staticInfo->isEnum) {
                return CheckJson(codec, json, UnderlyingType(info), nullptr, path, depth + 1);
            }
            if (info->containerInfo != nullptr && info->containerInfo->sequenceView != nullptr) {
                if (!json.IsArray()) {
                    return codec.Invalid(path, "Expected an array");
                }
                const auto *element = ElementType(info);
                for (rapidjson::SizeType i = 0; i < json.Size(); ++i) {
                    auto result = CheckJson(codec, json[i], element, nullptr, path + "[" + std::to_string(i) + "]", depth + 1);
                    if (!result.Succeeded()) {
                        return result;
                    }
                }
                return {};
            }
            if (!json.IsObject()) {
                return codec.Invalid(path, "Expected a reflected settings object");
            }
            const auto id = Find(json, "classId");
            if (id == json.MemberEnd() || !id->value.IsString() || id->value.GetStringLength() != 36 || id->value.GetString()[8] != '-' ||
                id->value.GetString()[13] != '-' || id->value.GetString()[18] != '-' || id->value.GetString()[23] != '-' ||
                Uuid::CreateFromString(std::string_view(id->value.GetString(), id->value.GetStringLength())) != info->registeredId) {
                return codec.Invalid(ChildPath(path, "classId"), "Settings type id does not match the registered type");
            }
            const auto elements = Find(json, "elements");
            if (elements == json.MemberEnd() || !elements->value.IsObject()) {
                return codec.Invalid(ChildPath(path, "elements"), "Expected the reflected elements object");
            }
            for (const auto &[name, member] : Node(info, hint)->members) {
                const auto entry = Find(elements->value, name);
                if (entry == elements->value.MemberEnd()) {
                    continue; // Absent fields retain provider defaults.
                }
                const auto memberPath = ChildPath(path, name);
                auto       result     = CheckJson(codec, entry->value, member.info, nullptr, memberPath, depth + 1);
                if (result.Succeeded()) {
                    result = CheckRange(codec, entry->value, member, memberPath);
                }
                if (!result.Succeeded()) {
                    return result;
                }
            }
            return {};
        }

        PreferenceResult
        ReadMember(const Codec &codec, const void *object, const serialize::TypeMemberNode &member, Any &value, const std::string &path)
        {
            auto result = ReflectionCall(codec, path, [&] {
                value = member.getterFn != nullptr ? member.getterFn(const_cast<void *>(object)) : member.getterConstFn(object);
            });
            if (!result.Succeeded()) {
                return result;
            }
            return value.Data() != nullptr && value.Info() == member.info ? PreferenceResult{}
                                                                          : codec.Invalid(path, "Getter returned an incompatible settings value");
        }

        template <typename T>
        T Number(const Json &json)
        {
            if constexpr (std::is_floating_point_v<T>) {
                return static_cast<T>(json.GetDouble());
            } else if constexpr (std::is_signed_v<T>) {
                return static_cast<T>(json.GetInt64());
            } else {
                return static_cast<T>(json.GetUint64());
            }
        }

        PreferenceResult
        Decode(const Codec &codec, const Json &json, void *object, const TypeInfoRT *info, const TypeNode *hint, const std::string &path)
        {
            if (info->registeredId == TypeInfo<bool>::RegisteredId()) {
                *static_cast<bool *>(object) = json.GetBool();
                return {};
            }
            if (info->registeredId == TypeInfo<std::string>::RegisteredId()) {
                *static_cast<std::string *>(object) = std::string(json.GetString(), json.GetStringLength());
                return {};
            }
            if (NumberType(info, [&](auto tag) {
                    const auto value = Number<typename decltype(tag)::Type>(json);
                    std::memcpy(object, &value, sizeof(value));
                })) {
                return {};
            }
            if (info->staticInfo->isEnum) {
                return Decode(codec, json, object, UnderlyingType(info), nullptr, path);
            }
            if (info->containerInfo != nullptr && info->containerInfo->sequenceView != nullptr) {
                SequenceVisitor sequence(info->containerInfo, object);
                std::size_t     count  = 0;
                auto            result = ReflectionCall(codec, path, [&] { count = sequence.Count(); });
                if (!result.Succeeded())
                    return result;
                while (count != 0) {
                    result = ReflectionCall(codec, path, [&] { sequence.Erase(count - 1); });
                    if (!result.Succeeded())
                        return result;
                    result = ReflectionCall(codec, path, [&] { count = sequence.Count(); });
                    if (!result.Succeeded())
                        return result;
                }
                const auto *element = ElementType(info);
                for (rapidjson::SizeType i = 0; i < json.Size(); ++i) {
                    void      *instance    = nullptr;
                    const auto elementPath = path + "[" + std::to_string(i) + "]";
                    result                 = ReflectionCall(codec, elementPath, [&] { instance = sequence.Emplace(); });
                    if (!result.Succeeded())
                        return result;
                    if (instance == nullptr) {
                        return codec.Unsupported(path, "Sequence could not construct an element");
                    }
                    result = Decode(codec, json[i], instance, element, nullptr, elementPath);
                    if (!result.Succeeded()) {
                        return result;
                    }
                }
                return codec.Check(path);
            }
            const auto &elements = Find(json, "elements")->value;
            for (const auto &[name, member] : Node(info, hint)->members) {
                const auto entry = Find(elements, name);
                if (entry == elements.MemberEnd() || member.isConst || member.setterFn == nullptr) {
                    continue;
                }
                const auto memberPath = ChildPath(path, name);
                Any        value;
                auto       result = ReadMember(codec, object, member, value, memberPath);
                if (result.Succeeded()) {
                    result = Decode(codec, entry->value, value.Data(), member.info, nullptr, memberPath);
                }
                if (!result.Succeeded()) {
                    return result;
                }
                bool written = false;
                result       = ReflectionCall(codec, memberPath, [&] { written = member.setterFn(object, value.Data()); });
                if (!result.Succeeded()) {
                    return result;
                }
                if (!written) {
                    return codec.Invalid(memberPath, "Settings candidate setter rejected a decoded value");
                }
            }
            return {};
        }

        PreferenceResult Encode(const Codec       &codec,
                                const void        *object,
                                const TypeInfoRT  *info,
                                const TypeNode    *hint,
                                Json              &json,
                                Allocator         &allocator,
                                const std::string &path,
                                unsigned           depth = 0)
        {
            if (depth > MaxDepth) {
                return codec.Invalid(path, "Settings nesting is too deep");
            }
            if (info->registeredId == TypeInfo<bool>::RegisteredId()) {
                json.SetBool(*static_cast<const bool *>(object));
                return {};
            }
            if (info->registeredId == TypeInfo<std::string>::RegisteredId()) {
                const auto &text = *static_cast<const std::string *>(object);
                json.SetString(text.data(), static_cast<rapidjson::SizeType>(text.size()), allocator);
                return {};
            }
            bool valid = true;
            if (NumberType(info, [&](auto tag) {
                    using T = typename decltype(tag)::Type;
                    T value;
                    std::memcpy(&value, object, sizeof(value));
                    if constexpr (std::is_floating_point_v<T>) {
                        valid = std::isfinite(value);
                        if (valid)
                            json.SetDouble(value);
                    } else if constexpr (std::is_signed_v<T>) {
                        json.SetInt64(value);
                    } else {
                        json.SetUint64(value);
                    }
                })) {
                return valid ? PreferenceResult{} : codec.Invalid(path, "Settings numbers must be finite");
            }
            if (info->staticInfo->isEnum) {
                return Encode(codec, object, UnderlyingType(info), nullptr, json, allocator, path, depth + 1);
            }
            if (info->containerInfo != nullptr && info->containerInfo->sequenceView != nullptr) {
                json.SetArray();
                // SequenceVisitor exposes no const view; only read through this owned snapshot.
                SequenceVisitor sequence(info->containerInfo, const_cast<void *>(object));
                const auto     *element = ElementType(info);
                std::size_t     count   = 0;
                auto            result  = ReflectionCall(codec, path, [&] { count = sequence.Count(); });
                if (!result.Succeeded())
                    return result;
                for (std::size_t i = 0; i < count; ++i) {
                    Json       item;
                    const auto elementPath = path + "[" + std::to_string(i) + "]";
                    void      *instance    = nullptr;
                    result                 = ReflectionCall(codec, elementPath, [&] { instance = sequence.GetByIndex(i); });
                    if (!result.Succeeded())
                        return result;
                    if (instance == nullptr)
                        return codec.Invalid(elementPath, "Sequence returned an inaccessible settings element");
                    result = Encode(codec, instance, element, nullptr, item, allocator, elementPath, depth + 1);
                    if (!result.Succeeded()) {
                        return result;
                    }
                    json.PushBack(item, allocator);
                }
                return codec.Check(path);
            }
            json.SetObject();
            const auto typeId = info->registeredId.ToString();
            Json       id;
            id.SetString(typeId.data(), static_cast<rapidjson::SizeType>(typeId.size()), allocator);
            Add(json, "classId", id, allocator);
            Json elements(rapidjson::kObjectType);
            for (const auto &[name, member] : Node(info, hint)->members) {
                const auto memberPath = ChildPath(path, name);
                Any        value;
                auto       result = ReadMember(codec, object, member, value, memberPath);
                Json       item;
                if (result.Succeeded()) {
                    result = Encode(codec, value.Data(), member.info, nullptr, item, allocator, memberPath, depth + 1);
                }
                if (result.Succeeded()) {
                    result = CheckRange(codec, item, member, memberPath);
                }
                if (!result.Succeeded()) {
                    return result;
                }
                Add(elements, name, item, allocator);
            }
            Add(json, "elements", elements, allocator);
            return {};
        }

        PreferenceResult CheckObjects(const Json &json, const std::string &path, unsigned depth = 0)
        {
            if (depth > MaxDepth) {
                return Error(PreferenceCode::InvalidJson, "Preferences JSON nesting is too deep", {}, path);
            }
            if (json.IsObject()) {
                std::unordered_set<std::string> keys;
                for (const auto &member : json.GetObject()) {
                    const std::string name(member.name.GetString(), member.name.GetStringLength());
                    if (!keys.insert(name).second) {
                        return Error(PreferenceCode::InvalidJson, "Duplicate JSON object key", {}, ChildPath(path, name));
                    }
                    auto result = CheckObjects(member.value, ChildPath(path, name), depth + 1);
                    if (!result.Succeeded())
                        return result;
                }
            } else if (json.IsArray()) {
                for (rapidjson::SizeType i = 0; i < json.Size(); ++i) {
                    auto result = CheckObjects(json[i], path + "[" + std::to_string(i) + "]", depth + 1);
                    if (!result.Succeeded())
                        return result;
                }
            }
            return {};
        }

        PreferenceResult Parse(const std::string &source, rapidjson::Document &document)
        {
            if (source.find('\0') != std::string::npos) {
                return Error(PreferenceCode::InvalidJson, "Preferences JSON contains an unescaped zero byte");
            }
            // Bound nesting before constructing the DOM. The iterative reader also
            // avoids recursive parser/destructor stacks for hostile deep input.
            struct DepthLimit : rapidjson::BaseReaderHandler<rapidjson::UTF8<>, DepthLimit> {
                unsigned depth = 0;
                bool     StartObject()
                {
                    return ++depth <= MaxDepth;
                }
                bool StartArray()
                {
                    return ++depth <= MaxDepth;
                }
                bool EndObject(rapidjson::SizeType)
                {
                    --depth;
                    return true;
                }
                bool EndArray(rapidjson::SizeType)
                {
                    --depth;
                    return true;
                }
            } limit;
            rapidjson::MemoryStream input(source.data(), source.size());
            rapidjson::Reader       reader;
            if (!reader.Parse < rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag > (input, limit)) {
                return Error(PreferenceCode::InvalidJson, std::string(rapidjson::GetParseError_En(reader.GetParseErrorCode())) + " at byte " +
                                                              std::to_string(reader.GetErrorOffset()));
            }
            document.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(source.data(), source.size());
            if (document.HasParseError()) {
                return Error(PreferenceCode::InvalidJson, std::string(rapidjson::GetParseError_En(document.GetParseError())) + " at byte " +
                                                              std::to_string(document.GetErrorOffset()));
            }
            if (!document.IsObject()) {
                return Error(PreferenceCode::InvalidJson, "Preferences document must be an object keyed by page id");
            }
            return CheckObjects(document, {});
        }

        void Merge(Json &target, const Json &source, Allocator &allocator)
        {
            if (!target.IsObject() || !source.IsObject()) {
                target.CopyFrom(source, allocator); // Known arrays replace their former contents.
                return;
            }
            for (const auto &member : source.GetObject()) {
                const std::string_view name(member.name.GetString(), member.name.GetStringLength());
                const auto             found = Find(target, name);
                if (found != target.MemberEnd()) {
                    Merge(found->value, member.value, allocator);
                } else {
                    Json value;
                    value.CopyFrom(member.value, allocator);
                    Add(target, name, value, allocator);
                }
            }
        }

        PreferenceResult CheckPages(const std::vector<PreferencePage> &pages)
        {
            std::unordered_set<std::string> ids;
            for (const auto &page : pages) {
                if (page.id.empty() || page.id.find('\0') != std::string::npos || !ids.insert(page.id).second) {
                    return Error(PreferenceCode::InvalidData, "Preferences page ids must be nonempty and unique", page.id);
                }
                if (page.object == nullptr || page.type == nullptr || page.type->info == nullptr || page.registration == 0) {
                    return Error(PreferenceCode::InvalidData, "Preferences page registration is incomplete", page.id);
                }
                if (page.type->info->staticInfo == nullptr || !page.type->info->staticInfo->isClass || page.type->info->containerInfo != nullptr ||
                    page.type->members.empty()) {
                    return Error(PreferenceCode::UnsupportedType, "A preferences page must be a reflected struct with readable fields", page.id);
                }
                std::unordered_set<const TypeInfoRT *> checked;
                auto                                   result = CheckType(Codec{page.id, {}}, page.type->info, page.type, {}, checked);
                if (!result.Succeeded())
                    return result;
            }
            return {};
        }

        bool SameRegistry(const PreferencePageRegistry &registry, const std::vector<PreferencePage> &snapshot)
        {
            const auto &live = registry.GetPages();
            if (live.size() != snapshot.size())
                return false;
            for (const auto &page : snapshot) {
                const auto found = std::find_if(live.begin(), live.end(), [&page](const auto &candidate) {
                    return candidate.id == page.id && candidate.registration == page.registration && candidate.object == page.object &&
                           candidate.type == page.type;
                });
                if (found == live.end())
                    return false;
            }
            return true;
        }

        template <typename Callback>
        PreferenceResult StorageCall(PreferenceCode failure, Callback &&callback)
        {
            PreferenceResult result;
            try {
                result = callback();
            }
            catch (const std::exception &error) {
                result = Error(failure, error.what());
            }
            catch (...) {
                result = Error(failure, "Preferences storage failed");
            }
            // The adapter performs storage I/O only. The service owns effect
            // accounting and marks writes after its post-callback guard.
            result.storageWritten     = false;
            result.liveWritten        = false;
            result.applicationApplied = false;
            if (failure == PreferenceCode::WriteFailed && result.code == PreferenceCode::NotFound)
                result.code = failure;
            return result;
        }

    } // namespace

    struct PreferencesPersistence::State {
        std::shared_ptr<IPreferenceStorage> storage;
        bool                                alive = true;
        bool                                busy  = false;

        struct Operation {
            std::shared_ptr<State> state;
            ~Operation()
            {
                state->busy = false;
            }
        };
    };

    PreferencesPersistence::PreferencesPersistence(std::shared_ptr<IPreferenceStorage> storage) : state(std::make_shared<State>())
    {
        state->storage = std::move(storage);
    }

    PreferencesPersistence::~PreferencesPersistence()
    {
        state->alive = false;
    }

    PreferenceResult PreferencesPersistence::Load(PreferencePageRegistry &registry)
    {
        const auto current = state;
        if (current->busy)
            return Error(PreferenceCode::Busy, "Preferences persistence is already running");
        current->busy = true;
        State::Operation operation{current};
        const auto       storage = current->storage;
        if (!storage)
            return Error(PreferenceCode::ReadFailed, "No Preferences storage is configured");
        const auto pages  = registry.GetPages();
        auto       result = CheckPages(pages);
        if (!result.Succeeded())
            return result;
        const auto  isCurrent = [current, &registry, pages] { return current->alive && SameRegistry(registry, pages); };
        const Codec guard{{}, isCurrent};
        result = guard.Check({});
        if (!result.Succeeded())
            return result;
        std::string source;
        const auto  read = StorageCall(PreferenceCode::ReadFailed, [&] { return storage->Read(source); });
        result           = guard.Check({});
        if (!result.Succeeded())
            return result;
        if (!read.Succeeded())
            return read;
        rapidjson::Document document;
        result = Parse(source, document);
        if (!result.Succeeded())
            return result;
        std::vector<PreferenceValue> values;
        result = CapturePreferenceValues(pages, values, isCurrent);
        if (!result.Succeeded())
            return result;
        result = guard.Check({});
        if (!result.Succeeded())
            return result;
        // Validate every registered payload before a candidate setter can run.
        for (const auto &value : values) {
            const auto found = Find(document, value.identity.id);
            if (found == document.MemberEnd())
                continue;
            const Codec codec{value.identity.id, isCurrent};
            result = CheckJson(codec, found->value, value.identity.type->info, value.identity.type, {});
            if (!result.Succeeded())
                return result;
        }
        for (auto &value : values) {
            const auto found = Find(document, value.identity.id);
            if (found == document.MemberEnd())
                continue;
            const Codec codec{value.identity.id, isCurrent};
            result = Decode(codec, found->value, value.value.Data(), value.identity.type->info, value.identity.type, {});
            if (!result.Succeeded())
                return result;
        }
        result = ValidatePreferenceValues(registry, values, isCurrent);
        if (!result.Succeeded())
            return result;
        result = guard.Check({});
        if (!result.Succeeded())
            return result;
        return PublishPreferenceValues(registry, values, isCurrent);
    }

    PreferenceResult PreferencesPersistence::Save(const std::vector<PreferenceValue> &values, const std::function<bool()> &isCurrent)
    {
        const auto current = state;
        if (current->busy)
            return Error(PreferenceCode::Busy, "Preferences persistence is already running");
        current->busy = true;
        State::Operation operation{current};
        const auto       storage = current->storage;
        if (!storage)
            return Error(PreferenceCode::WriteFailed, "No Preferences storage is configured");
        const auto check        = isCurrent;
        const auto stillCurrent = [current, check] {
            if (!current->alive || (check && !check()))
                return false;
            return current->alive;
        };
        const Codec guard{{}, stillCurrent};
        auto        result = guard.Check({});
        if (!result.Succeeded())
            return result;
        std::vector<PreferencePage> pages;
        for (const auto &value : values) {
            pages.push_back(value.identity);
            if (value.value.Data() == nullptr || value.identity.type == nullptr || value.value.Info() != value.identity.type->info) {
                return Error(PreferenceCode::InvalidData, "Preferences candidate has an incompatible type", value.identity.id);
            }
        }
        result = CheckPages(pages);
        if (!result.Succeeded())
            return result;
        // Own the input across storage/getter callbacks that can destroy their caller.
        auto sourcePages = pages;
        for (std::size_t i = 0; i < sourcePages.size(); ++i) {
            sourcePages[i].object = const_cast<void *>(values[i].value.Data());
        }
        std::vector<PreferenceValue> snapshots;
        result = CapturePreferenceValues(sourcePages, snapshots, stillCurrent);
        if (!result.Succeeded())
            return result;
        for (std::size_t i = 0; i < snapshots.size(); ++i) {
            snapshots[i].identity = pages[i];
        }
        result = guard.Check({});
        if (!result.Succeeded())
            return result;
        std::string source;
        const auto  read = StorageCall(PreferenceCode::ReadFailed, [&] { return storage->Read(source); });
        result           = guard.Check({});
        if (!result.Succeeded())
            return result;
        rapidjson::Document document;
        if (read.code == PreferenceCode::NotFound) {
            document.SetObject();
        } else {
            if (!read.Succeeded())
                return read;
            result = Parse(source, document);
            if (!result.Succeeded())
                return result;
        }
        rapidjson::Document encoded;
        encoded.SetObject();
        for (const auto &value : snapshots) {
            const Codec codec{value.identity.id, stillCurrent};
            const auto  found = Find(document, value.identity.id);
            if (found != document.MemberEnd()) {
                result = CheckJson(codec, found->value, value.identity.type->info, value.identity.type, {});
                if (!result.Succeeded())
                    return result;
            }
            Json page;
            result = Encode(codec, value.value.Data(), value.identity.type->info, value.identity.type, page, encoded.GetAllocator(), {});
            if (!result.Succeeded())
                return result;
            Add(encoded, value.identity.id, page, encoded.GetAllocator());
        }
        Merge(document, encoded, document.GetAllocator());
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator,
                          rapidjson::kWriteValidateEncodingFlag>
            writer(buffer);
        if (!document.Accept(writer))
            return Error(PreferenceCode::InvalidData, "Preferences document could not be encoded");
        const std::string output(buffer.GetString(), buffer.GetSize());
        result = guard.Check({});
        if (!result.Succeeded())
            return result;
        const auto written = StorageCall(PreferenceCode::WriteFailed, [&] { return storage->Write(output); });
        result             = guard.Check({});
        if (!result.Succeeded()) {
            result.storageWritten = written.Succeeded();
            return result;
        }
        if (!written.Succeeded())
            return written;
        result.storageWritten = true;
        return result;
    }
} // namespace sky::editor
