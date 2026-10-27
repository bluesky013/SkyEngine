#include "PhysicsAssetCodec.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>

namespace sky::phy {
    namespace {
        constexpr std::string_view NAMES[]      = {"Box", "Sphere", "Capsule", "HeightField", "TriangleMesh", "ConvexHull", "Compound"};
        constexpr size_t           MAX_ELEMENTS = 4096;
        constexpr size_t           MAX_VALUES   = 4u * 1024u * 1024u;
        using Value                             = rapidjson::Value;
        using Allocator                         = rapidjson::Document::AllocatorType;

        PhysicsStatus Invalid(const char *message)
        {
            return {PhysicsError::InvalidArgument, message};
        }

        const Value *Member(const Value &object, const char *name)
        {
            if (!object.IsObject())
                return nullptr;
            const auto member = object.FindMember(name);
            return member == object.MemberEnd() ? nullptr : &member->value;
        }

        bool Float(const Value &value, float &out)
        {
            if (!value.IsNumber() || !std::isfinite(value.GetDouble()) || std::abs(value.GetDouble()) > std::numeric_limits<float>::max())
                return false;
            out = static_cast<float>(value.GetDouble());
            return true;
        }

        bool Field(const Value &object, const char *name, float &out)
        {
            const auto *value = Member(object, name);
            return !value || Float(*value, out);
        }

        bool Vector(const Value &value, float *out, size_t count)
        {
            if (!value.IsArray() || value.Size() != count)
                return false;
            for (size_t i = 0; i < count; ++i)
                if (!Float(value[static_cast<rapidjson::SizeType>(i)], out[i]))
                    return false;
            return true;
        }

        bool VectorField(const Value &object, const char *name, float *out, size_t count)
        {
            const auto *value = Member(object, name);
            return !value || Vector(*value, out, count);
        }

        bool UInt(const Value &object, const char *name, uint32_t &out)
        {
            const auto *value = Member(object, name);
            if (!value || !value->IsUint())
                return false;
            out = value->GetUint();
            return true;
        }

        bool Parse(std::string_view text, rapidjson::Document &document)
        {
            if (text.empty() || text.size() > MAX_PHYSICS_PRODUCT_BYTES)
                return false;
            document.Parse<rapidjson::kParseIterativeFlag>(text.data(), text.size());
            uint32_t version = 0;
            return !document.HasParseError() && UInt(document, "version", version) && version == 1;
        }

        Value Array(const float *values, size_t count, Allocator &allocator)
        {
            Value array(rapidjson::kArrayType);
            for (size_t i = 0; i < count; ++i)
                array.PushBack(values[i], allocator);
            return array;
        }

        bool Text(rapidjson::Document &document, std::string &out)
        {
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            if (!document.Accept(writer))
                return false;
            if (buffer.GetSize() > MAX_PHYSICS_PRODUCT_BYTES)
                return false;
            out.assign(buffer.GetString(), buffer.GetSize());
            return true;
        }
    } // namespace

    bool ReadPhysicsText(std::istream &stream, std::string &out)
    {
        std::string result;
        char        buffer[4096];
        while (stream.read(buffer, sizeof(buffer)) || stream.gcount()) {
            const auto count = static_cast<size_t>(stream.gcount());
            if (count > MAX_PHYSICS_PRODUCT_BYTES - result.size())
                return false;
            result.append(buffer, count);
        }
        if (stream.bad() || !stream.eof() || result.empty())
            return false;
        stream.clear();
        out = std::move(result);
        return true;
    }

    PhysicsStatus ParseCollision(std::string_view text, CollisionSource &out)
    {
        rapidjson::Document document;
        if (!Parse(text, document) || !Member(document, "shape"))
            return Invalid("Collision requires version 1 and a shape");
        CollisionSource result;
        struct Pending {
            const Value *value;
            ShapeDesc   *shape;
            uint32_t     depth;
        };
        std::vector<Pending> pending{{Member(document, "shape"), &result.shape, 0}};
        std::set<Uuid>       identities;
        size_t               values = 0;
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            const auto &value = *current.value;
            auto       &shape = *current.shape;
            const auto *type  = Member(value, "type");
            const auto *id    = Member(value, "id");
            if (current.depth > 32 || identities.size() >= MAX_ELEMENTS || !type || !type->IsString() || !id || !id->IsString())
                return Invalid("Collision element requires an id/type within the nesting and element budget");
            const auto identity = Uuid::CreateFromString({id->GetString(), id->GetStringLength()});
            if (!identity || !identities.insert(identity).second)
                return Invalid("Collision element ids must be valid and unique");
            result.elements.push_back(identity);
            const auto found = std::find(std::begin(NAMES), std::end(NAMES), std::string_view(type->GetString(), type->GetStringLength()));
            if (found == std::end(NAMES))
                return Invalid("Unknown collision shape type");
            shape.type = static_cast<ShapeType>(found - std::begin(NAMES));
            if (!VectorField(value, "pivot", &shape.pivot.x, 3) || !VectorField(value, "rotation", shape.rotation.v, 4))
                return Invalid("Invalid collision local transform");
            switch (shape.type) {
            case ShapeType::Box:
                if (!VectorField(value, "halfExt", &shape.halfExt.x, 3))
                    return Invalid("Invalid box size");
                break;
            case ShapeType::Sphere:
            case ShapeType::Capsule:
                if (!Field(value, "radius", shape.radius) || !Field(value, "height", shape.height))
                    return Invalid("Invalid sphere/capsule size");
                break;
            case ShapeType::HeightField: {
                const auto *samples = Member(value, "samples");
                uint32_t    up      = 1;
                if (!UInt(value, "cols", shape.cols) || !UInt(value, "rows", shape.rows) || !samples || !samples->IsArray() ||
                    uint64_t(shape.cols) * shape.rows != samples->Size() || (Member(value, "upAxis") && !UInt(value, "upAxis", up)) || up > 2 ||
                    !Field(value, "scaleX", shape.scaleX) || !Field(value, "scaleZ", shape.scaleZ) ||
                    !Field(value, "heightScale", shape.heightScale) || !Field(value, "heightOffset", shape.heightOffset))
                    return Invalid("Invalid heightfield dimensions or samples");
                shape.upAxis = static_cast<uint8_t>(up);
                values += samples->Size();
                if (values > MAX_VALUES)
                    return Invalid("Collision sample budget exceeded");
                shape.samples.resize(samples->Size());
                for (rapidjson::SizeType i = 0; i < samples->Size(); ++i)
                    if (!Float((*samples)[i], shape.samples[i]))
                        return Invalid("Invalid height sample");
                break;
            }
            case ShapeType::ConvexHull:
            case ShapeType::TriangleMesh: {
                const auto *points = Member(value, shape.type == ShapeType::ConvexHull ? "points" : "vertices");
                if (!points || !points->IsArray())
                    return Invalid("Collision geometry requires points/vertices");
                values += uint64_t(points->Size()) * 3;
                if (values > MAX_VALUES)
                    return Invalid("Collision vertex budget exceeded");
                std::vector<Vector3> vertices(points->Size());
                for (rapidjson::SizeType i = 0; i < points->Size(); ++i)
                    if (!Vector((*points)[i], &vertices[i].x, 3))
                        return Invalid("Invalid collision vertex");
                if (shape.type == ShapeType::ConvexHull) {
                    shape.points = std::move(vertices);
                    break;
                }
                const auto *indices = Member(value, "indices");
                if (!indices || !indices->IsArray() || indices->Empty() || indices->Size() % 3 != 0)
                    return Invalid("Collision mesh requires triangle indices");
                values += indices->Size();
                if (values > MAX_VALUES)
                    return Invalid("Collision index budget exceeded");
                shape.meshData = new TriangleMesh();
                shape.meshData->position.resize(vertices.size() * sizeof(Vector3));
                std::memcpy(shape.meshData->position.data(), vertices.data(), shape.meshData->position.size());
                shape.meshData->indexRaw.resize(size_t(indices->Size()) * sizeof(uint32_t));
                for (rapidjson::SizeType i = 0; i < indices->Size(); ++i) {
                    if (!(*indices)[i].IsUint() || (*indices)[i].GetUint() >= vertices.size())
                        return Invalid("Collision mesh index is out of range");
                    const uint32_t index = (*indices)[i].GetUint();
                    std::memcpy(shape.meshData->indexRaw.data() + size_t(i) * sizeof(index), &index, sizeof(index));
                }
                break;
            }
            case ShapeType::Compound: {
                const auto *children = Member(value, "children");
                if (!children || !children->IsArray() || children->Empty() || children->Size() > MAX_ELEMENTS - identities.size() - pending.size())
                    return Invalid("Invalid compound children");
                shape.children.resize(children->Size());
                for (size_t i = shape.children.size(); i > 0; --i)
                    pending.push_back({&(*children)[static_cast<rapidjson::SizeType>(i - 1)], &shape.children[i - 1], current.depth + 1});
                break;
            }
            }
        }
        std::string error;
        if (!ValidateShape(result.shape, &error))
            return {PhysicsError::InvalidArgument, std::move(error)};
        out = std::move(result);
        return {};
    }

    PhysicsStatus WriteCollision(const CollisionSource &source, std::string &out)
    {
        size_t                         nodes = 0, numericValues = 0;
        std::vector<const ShapeDesc *> scan{&source.shape};
        while (!scan.empty()) {
            const auto *shape = scan.back();
            scan.pop_back();
            if (++nodes > MAX_ELEMENTS || scan.size() + shape->children.size() > MAX_ELEMENTS)
                return Invalid("Collision element budget exceeded");
            numericValues += shape->samples.size() + shape->points.size() * 3;
            if (shape->meshData)
                numericValues += shape->meshData->position.size() / sizeof(float) + shape->meshData->indexRaw.size() / 2;
            if (numericValues > MAX_VALUES)
                return Invalid("Collision geometry budget exceeded");
            if (shape->type == ShapeType::Compound)
                for (const auto &child : shape->children)
                    scan.push_back(&child);
        }
        rapidjson::Document document(rapidjson::kObjectType);
        auto               &allocator = document.GetAllocator();
        document.AddMember("version", 1, allocator);
        document.AddMember("shape", Value(rapidjson::kObjectType), allocator);
        struct Pending {
            const ShapeDesc *shape;
            Value           *value;
            uint32_t         depth;
        };
        std::vector<Pending> pending{{&source.shape, &document["shape"], 0}};
        size_t               element = 0;
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            const auto &shape = *current.shape;
            auto       &value = *current.value;
            if (element >= source.elements.size() || element >= MAX_ELEMENTS || current.depth > 32 || shape.type > ShapeType::Compound)
                return Invalid("Invalid collision element identities or depth");
            const auto id = source.elements[element++].ToString();
            value.AddMember("id", Value(id.c_str(), allocator), allocator);
            value.AddMember("type", Value(NAMES[static_cast<size_t>(shape.type)].data(), allocator), allocator);
            value.AddMember("pivot", Array(&shape.pivot.x, 3, allocator), allocator);
            value.AddMember("rotation", Array(shape.rotation.v, 4, allocator), allocator);
            switch (shape.type) {
            case ShapeType::Box: value.AddMember("halfExt", Array(&shape.halfExt.x, 3, allocator), allocator); break;
            case ShapeType::Sphere:
            case ShapeType::Capsule:
                value.AddMember("radius", shape.radius, allocator);
                value.AddMember("height", shape.height, allocator);
                break;
            case ShapeType::HeightField:
                value.AddMember("cols", shape.cols, allocator);
                value.AddMember("rows", shape.rows, allocator);
                value.AddMember("scaleX", shape.scaleX, allocator);
                value.AddMember("scaleZ", shape.scaleZ, allocator);
                value.AddMember("heightScale", shape.heightScale, allocator);
                value.AddMember("heightOffset", shape.heightOffset, allocator);
                value.AddMember("upAxis", shape.upAxis, allocator);
                if (shape.samples.size() > MAX_VALUES)
                    return Invalid("Collision sample budget exceeded");
                value.AddMember("samples", Array(shape.samples.data(), shape.samples.size(), allocator), allocator);
                break;
            case ShapeType::ConvexHull: {
                if (shape.points.size() > MAX_VALUES / 3)
                    return Invalid("Collision point budget exceeded");
                Value points(rapidjson::kArrayType);
                for (const auto &point : shape.points)
                    points.PushBack(Array(&point.x, 3, allocator), allocator);
                value.AddMember("points", points, allocator);
                break;
            }
            case ShapeType::TriangleMesh: {
                const auto &mesh = shape.meshData;
                if (!mesh || mesh->vtxStride < sizeof(Vector3) || mesh->position.size() % mesh->vtxStride || mesh->indexType > IndexType::U32 ||
                    mesh->position.size() > MAX_PHYSICS_PRODUCT_BYTES || mesh->indexRaw.size() > MAX_PHYSICS_PRODUCT_BYTES)
                    return Invalid("Collision mesh CPU geometry is invalid");
                Value vertices(rapidjson::kArrayType), indices(rapidjson::kArrayType);
                for (size_t i = 0; i < mesh->position.size(); i += mesh->vtxStride) {
                    Vector3 point;
                    std::memcpy(&point, mesh->position.data() + i, sizeof(point));
                    vertices.PushBack(Array(&point.x, 3, allocator), allocator);
                }
                const size_t stride = mesh->indexType == IndexType::U16 ? 2 : 4;
                if (mesh->indexRaw.size() % stride)
                    return Invalid("Collision mesh index size is invalid");
                for (size_t i = 0; i < mesh->indexRaw.size(); i += stride) {
                    uint32_t index = 0;
                    std::memcpy(&index, mesh->indexRaw.data() + i, stride);
                    indices.PushBack(index, allocator);
                }
                value.AddMember("vertices", vertices, allocator);
                value.AddMember("indices", indices, allocator);
                break;
            }
            case ShapeType::Compound: {
                if (shape.children.size() > MAX_ELEMENTS)
                    return Invalid("Collision child budget exceeded");
                Value children(rapidjson::kArrayType);
                for (size_t i = 0; i < shape.children.size(); ++i)
                    children.PushBack(Value(rapidjson::kObjectType), allocator);
                value.AddMember("children", children, allocator);
                for (size_t i = shape.children.size(); i > 0; --i)
                    pending.push_back({&shape.children[i - 1], &value["children"][static_cast<rapidjson::SizeType>(i - 1)], current.depth + 1});
                break;
            }
            }
        }
        if (element != source.elements.size())
            return Invalid("Collision element count differs from geometry");
        std::string encoded;
        if (!Text(document, encoded))
            return Invalid("Non-finite or oversized physics product");
        CollisionSource validated;
        if (auto status = ParseCollision(encoded, validated); !status)
            return status;
        out = std::move(encoded);
        return {};
    }

    PhysicsStatus ParsePhysicsMaterial(std::string_view text, PhysicsMaterialData &out)
    {
        rapidjson::Document document;
        PhysicsMaterialData result;
        uint32_t            frictionCombine = 0, restitutionCombine = 0;
        if (!Parse(text, document) || Member(document, "staticFriction") || Member(document, "dynamicFriction") ||
            Member(document, "linearDamping") || Member(document, "angularDamping") || !Field(document, "friction", result.dynamicFriction) ||
            result.dynamicFriction < 0 || !Field(document, "restitution", result.restitution) || result.restitution < 0 || result.restitution > 1 ||
            (Member(document, "frictionCombine") && !UInt(document, "frictionCombine", frictionCombine)) ||
            (Member(document, "restitutionCombine") && !UInt(document, "restitutionCombine", restitutionCombine)) || frictionCombine > 3 ||
            restitutionCombine > 3)
            return Invalid("Invalid version 1 physics material");
        result.staticFriction     = result.dynamicFriction;
        result.frictionCombine    = static_cast<PhysicsCombineMode>(frictionCombine);
        result.restitutionCombine = static_cast<PhysicsCombineMode>(restitutionCombine);
        out                       = result;
        return {};
    }

    PhysicsStatus WritePhysicsMaterial(const PhysicsMaterialData &source, std::string &out)
    {
        if (!std::isfinite(source.dynamicFriction) || !std::isfinite(source.restitution) || source.staticFriction != source.dynamicFriction ||
            source.linearDamping != 0 || source.angularDamping != 0)
            return Invalid("Material assets require one friction coefficient and body-owned damping");
        rapidjson::Document document(rapidjson::kObjectType);
        auto               &allocator = document.GetAllocator();
        document.AddMember("version", 1, allocator);
        document.AddMember("friction", source.dynamicFriction, allocator);
        document.AddMember("restitution", source.restitution, allocator);
        document.AddMember("frictionCombine", static_cast<uint32_t>(source.frictionCombine), allocator);
        document.AddMember("restitutionCombine", static_cast<uint32_t>(source.restitutionCombine), allocator);
        std::string encoded;
        if (!Text(document, encoded))
            return Invalid("Non-finite or oversized physics product");
        PhysicsMaterialData validated;
        if (auto status = ParsePhysicsMaterial(encoded, validated); !status)
            return status;
        out = std::move(encoded);
        return {};
    }
} // namespace sky::phy
