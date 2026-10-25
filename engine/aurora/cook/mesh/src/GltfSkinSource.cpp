#include "GltfSkinSource.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <span>

namespace sky::aurora::cook {
    namespace {
        using Json                  = rapidjson::Value;
        constexpr size_t MAX_BUFFER = 256U * 1024U * 1024U;
        MeshCookStatus   Invalid(std::string message)
        {
            return {MeshCookError::InvalidData, "glTF Skin: " + std::move(message)};
        }
        bool Uint(const Json &value, const char *key, uint32_t &out, uint32_t fallback = UINT32_MAX)
        {
            if (!value.IsObject()) {
                return false;
            }
            const auto found = value.FindMember(key);
            if (found == value.MemberEnd()) {
                out = fallback;
                return fallback != UINT32_MAX;
            }
            if (!found->value.IsUint()) {
                return false;
            }
            out = found->value.GetUint();
            return true;
        }
        uint32_t Word(const uint8_t *bytes)
        {
            return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
        }
        bool DecodeDataUri(const std::string &uri, std::vector<uint8_t> &out)
        {
            const auto delimiter = uri.find(";base64,");
            if (!uri.starts_with("data:") || delimiter == std::string::npos) {
                return false;
            }
            const std::string_view text(uri.data() + delimiter + 8, uri.size() - delimiter - 8);
            if (text.size() % 4 || text.size() / 4 * 3 > MAX_BUFFER) {
                return false;
            }
            const auto digit = [](char value) -> int {
                if (value >= 'A' && value <= 'Z') {
                    return value - 'A';
                }
                if (value >= 'a' && value <= 'z') {
                    return value - 'a' + 26;
                }
                if (value >= '0' && value <= '9') {
                    return value - '0' + 52;
                }
                return value == '+' ? 62 : value == '/' ? 63 : -1;
            };
            for (size_t offset = 0; offset < text.size(); offset += 4) {
                uint32_t word    = 0;
                uint32_t padding = 0;
                for (size_t i = 0; i < 4; ++i) {
                    const char value = text[offset + i];
                    if (value == '=') {
                        if (offset + 4 != text.size() || i < 2) {
                            return false;
                        }
                        ++padding;
                        word <<= 6;
                    } else {
                        const int decoded = digit(value);
                        if (decoded < 0 || padding) {
                            return false;
                        }
                        word = word << 6 | uint32_t(decoded);
                    }
                }
                if (padding > 2) {
                    return false;
                }
                out.push_back(uint8_t(word >> 16));
                if (padding < 2) {
                    out.push_back(uint8_t(word >> 8));
                }
                if (!padding) {
                    out.push_back(uint8_t(word));
                }
            }
            return true;
        }
        struct Accessor {
            const uint8_t *data  = nullptr;
            uint32_t       count = 0, stride = 0, component = 0, size = 0;
            bool           normalized = false;
            double         Read(uint32_t vertex, uint32_t lane) const
            {
                const auto *value = data + size_t(vertex) * stride + lane * size;
                if (component == 5126) {
                    float result = 0;
                    std::memcpy(&result, value, 4);
                    return result;
                }
                const uint32_t result = size == 1 ? value[0] : uint32_t(value[0]) | uint32_t(value[1]) << 8;
                return normalized ? double(result) / (size == 1 ? 255.0 : 65535.0) : result;
            }
        };
        MeshCookStatus ReadAccessor(const Json &document, const std::vector<std::vector<uint8_t>> &buffers, uint32_t id, bool weights, Accessor &out)
        {
            if (!document.HasMember("accessors") || !document["accessors"].IsArray() || id >= document["accessors"].Size() ||
                !document.HasMember("bufferViews") || !document["bufferViews"].IsArray()) {
                return Invalid("influence accessor is missing");
            }
            const auto &accessor = document["accessors"][id];
            if (!accessor.IsObject() || accessor.HasMember("sparse")) {
                return {MeshCookError::Unsupported, "sparse Skin influence accessors are unsupported"};
            }
            uint32_t viewId = 0, offset = 0;
            if (!Uint(accessor, "bufferView", viewId) || !Uint(accessor, "byteOffset", offset, 0) || !Uint(accessor, "count", out.count) ||
                !Uint(accessor, "componentType", out.component) || !out.count || out.count > 16U * 1024U * 1024U ||
                viewId >= document["bufferViews"].Size() || !accessor.HasMember("type") || !accessor["type"].IsString() ||
                std::string_view(accessor["type"].GetString()) != "VEC4") {
                return Invalid("invalid influence accessor");
            }
            out.size = out.component == 5121 ? 1 : out.component == 5123 ? 2 : out.component == 5126 && weights ? 4 : 0;
            if (accessor.HasMember("normalized")) {
                if (!accessor["normalized"].IsBool()) {
                    return Invalid("invalid accessor normalization");
                }
                out.normalized = accessor["normalized"].GetBool();
            }
            if (!out.size || (!weights && out.normalized) || (weights && out.component != 5126 && !out.normalized) ||
                (out.component == 5126 && out.normalized)) {
                return Invalid("invalid influence component format");
            }
            const auto &view   = document["bufferViews"][viewId];
            uint32_t    buffer = 0, begin = 0, length = 0;
            if (!Uint(view, "buffer", buffer) || buffer >= buffers.size() || !Uint(view, "byteOffset", begin, 0) ||
                !Uint(view, "byteLength", length) || !Uint(view, "byteStride", out.stride, out.size * 4) || out.stride < out.size * 4 ||
                out.stride > 252 || out.stride % out.size || begin > buffers[buffer].size() || length > buffers[buffer].size() - begin ||
                offset > length || uint64_t(out.count - 1) * out.stride + out.size * 4 > length - offset) {
                return Invalid("influence buffer range is invalid");
            }
            out.data = buffers[buffer].data() + begin + offset;
            return {};
        }
        MeshCookStatus
        ValidatePrimitive(const Json &document, const Json &primitive, const std::vector<std::vector<uint8_t>> &buffers, uint32_t jointCount)
        {
            if (!primitive.IsObject() || !primitive.HasMember("attributes") || !primitive["attributes"].IsObject()) {
                return Invalid("primitive attributes are missing");
            }
            const auto                                       &attributes = primitive["attributes"];
            std::map<uint32_t, std::pair<uint32_t, uint32_t>> sets;
            for (auto member = attributes.MemberBegin(); member != attributes.MemberEnd(); ++member) {
                const std::string_view name(member->name.GetString(), member->name.GetStringLength());
                const bool             joints = name.starts_with("JOINTS_");
                if (!joints && !name.starts_with("WEIGHTS_")) {
                    continue;
                }
                const auto suffix = name.substr(joints ? 7 : 8);
                uint32_t   number = 0;
                if (suffix.empty() || suffix.size() > 2 || !member->value.IsUint()) {
                    return Invalid("invalid influence attribute set");
                }
                for (const char value : suffix) {
                    if (value < '0' || value > '9') {
                        return Invalid("invalid influence attribute suffix");
                    }
                    number = number * 10 + value - '0';
                }
                auto [entry, inserted] = sets.try_emplace(number, UINT32_MAX, UINT32_MAX);
                auto &id               = joints ? entry->second.first : entry->second.second;
                if (id != UINT32_MAX) {
                    return Invalid("duplicate influence attribute");
                }
                id = member->value.GetUint();
            }
            if (!sets.contains(0)) {
                return Invalid("skinned primitive has no JOINTS_0/WEIGHTS_0");
            }
            std::vector<std::pair<Accessor, Accessor>> readers;
            uint32_t                                   count = 0;
            for (const auto &[number, ids] : sets) {
                Accessor joints, weights;
                if (const auto status = ReadAccessor(document, buffers, ids.first, false, joints); !status) {
                    return status;
                }
                if (const auto status = ReadAccessor(document, buffers, ids.second, true, weights); !status) {
                    return status;
                }
                if (joints.count != weights.count || (count && count != joints.count)) {
                    return Invalid("influence accessor counts differ");
                }
                count = joints.count;
                readers.emplace_back(joints, weights);
            }
            for (uint32_t vertex = 0; vertex < count; ++vertex) {
                std::map<uint32_t, double> influences;
                for (const auto &[joints, weights] : readers) {
                    for (uint32_t lane = 0; lane < 4; ++lane) {
                        const auto   joint  = uint32_t(joints.Read(vertex, lane));
                        const double weight = weights.Read(vertex, lane);
                        if (joint >= jointCount || !std::isfinite(weight) || weight < 0) {
                            return Invalid("original influence is negative, nonfinite or out of range");
                        }
                        if (weight > 0) {
                            influences[joint] += weight;
                        }
                    }
                }
                if (influences.empty()) {
                    return Invalid("original vertex has zero total influence");
                }
                if (influences.size() > 4) {
                    return {MeshCookError::Unsupported, "glTF vertex has more than four positive influences"};
                }
            }
            return {};
        }
    } // namespace

    MeshCookStatus PrepareGltfSkinSource(const std::vector<uint8_t> &bytes, const std::string &extension, std::vector<uint8_t> &out)
    {
        if (extension != "gltf" && extension != "glb") {
            out = bytes;
            return {};
        }
        const bool               glb     = extension == "glb";
        std::span<const uint8_t> json    = bytes, binary;
        size_t                   jsonEnd = 0;
        if (glb) {
            if (bytes.size() < 20 || Word(bytes.data()) != 0x46546C67 || Word(bytes.data() + 4) != 2 || Word(bytes.data() + 8) != bytes.size() ||
                Word(bytes.data() + 16) != 0x4E4F534A || Word(bytes.data() + 12) > bytes.size() - 20) {
                return Invalid("invalid GLB header");
            }
            json    = {bytes.data() + 20, Word(bytes.data() + 12)};
            jsonEnd = 20 + json.size();
            if (bytes.size() > jsonEnd) {
                if (bytes.size() - jsonEnd < 8 || Word(bytes.data() + jsonEnd + 4) != 0x004E4942 ||
                    Word(bytes.data() + jsonEnd) > bytes.size() - jsonEnd - 8) {
                    return Invalid("invalid GLB binary chunk");
                }
                binary = {bytes.data() + jsonEnd + 8, Word(bytes.data() + jsonEnd)};
            }
        }
        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char *>(json.data()), json.size());
        if (document.HasParseError() || !document.IsObject()) {
            return Invalid("invalid JSON document");
        }
        if (!document.HasMember("skins")) {
            out = bytes;
            return {};
        }
        if (!document["skins"].IsArray() || !document.HasMember("nodes") || !document["nodes"].IsArray() || !document.HasMember("meshes") ||
            !document["meshes"].IsArray() || !document.HasMember("buffers") || !document["buffers"].IsArray()) {
            return Invalid("missing Skin source tables");
        }
        std::vector<std::vector<uint8_t>> buffers;
        for (const auto &buffer : document["buffers"].GetArray()) {
            uint32_t length = 0;
            if (!Uint(buffer, "byteLength", length) || length > MAX_BUFFER) {
                return Invalid("invalid buffer size");
            }
            std::vector<uint8_t> decoded;
            if (buffer.HasMember("uri")) {
                if (!buffer["uri"].IsString() || !DecodeDataUri(buffer["uri"].GetString(), decoded)) {
                    return {MeshCookError::Unsupported, "Skin cook requires embedded glTF data URIs or GLB binary; external buffers are unsupported"};
                }
            } else if (glb && buffers.empty()) {
                decoded.assign(binary.begin(), binary.end());
            } else {
                return Invalid("buffer data is absent");
            }
            if (decoded.size() < length || decoded.size() - length > (glb ? 3 : 0)) {
                return Invalid("buffer byteLength differs from its payload");
            }
            decoded.resize(length);
            buffers.push_back(std::move(decoded));
        }
        for (auto &node : document["nodes"].GetArray()) {
            if (!node.IsObject() || !node.HasMember("skin")) {
                continue;
            }
            uint32_t skinId = 0, meshId = 0;
            if (!Uint(node, "skin", skinId) || !Uint(node, "mesh", meshId) || skinId >= document["skins"].Size() ||
                meshId >= document["meshes"].Size()) {
                return Invalid("invalid mesh/Skin reference");
            }
            const auto &skin = document["skins"][skinId];
            if (!skin.IsObject() || !skin.HasMember("joints") || !skin["joints"].IsArray() || skin["joints"].Empty() || skin["joints"].Size() > 256) {
                return Invalid("Skin must have 1..256 joints");
            }
            std::set<uint32_t> joints;
            for (const auto &joint : skin["joints"].GetArray()) {
                if (!joint.IsUint() || joint.GetUint() >= document["nodes"].Size() || !joints.insert(joint.GetUint()).second) {
                    return Invalid("invalid or duplicated joint reference");
                }
            }
            const auto &mesh = document["meshes"][meshId];
            if (!mesh.IsObject() || !mesh.HasMember("primitives") || !mesh["primitives"].IsArray()) {
                return Invalid("mesh primitive table is absent");
            }
            for (const auto &primitive : mesh["primitives"].GetArray()) {
                if (const auto status = ValidatePrimitive(document, primitive, buffers, uint32_t(joints.size())); !status) {
                    return status;
                }
            }
            // Assimp associates bones with aiMesh; clone before import so another node's Skin cannot overwrite this binding.
            Json       copy(mesh, document.GetAllocator());
            const auto duplicate = document["meshes"].Size();
            document["meshes"].PushBack(copy, document.GetAllocator());
            node["mesh"].SetUint(duplicate);
        }
        rapidjson::StringBuffer                    buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
        if (!glb) {
            out.assign(buffer.GetString(), buffer.GetString() + buffer.GetSize());
            return {};
        }
        const auto padded = (buffer.GetSize() + 3) & ~size_t{3};
        const auto total  = 20 + padded + bytes.size() - jsonEnd;
        if (total > UINT32_MAX) {
            return Invalid("expanded GLB exceeds format size");
        }
        out.assign(total, 0x20);
        const auto write = [&out](size_t offset, uint32_t value) {
            for (size_t i = 0; i < 4; ++i) {
                out[offset + i] = uint8_t(value >> (i * 8));
            }
        };
        write(0, 0x46546C67);
        write(4, 2);
        write(8, uint32_t(total));
        write(12, uint32_t(padded));
        write(16, 0x4E4F534A);
        std::memcpy(out.data() + 20, buffer.GetString(), buffer.GetSize());
        std::memcpy(out.data() + 20 + padded, bytes.data() + jsonEnd, bytes.size() - jsonEnd);
        return {};
    }
} // namespace sky::aurora::cook
