#include <softbody/assets/SoftBodyRecipe.h>

#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace sky::softbody {
    namespace {
        constexpr size_t MAX_RECIPE_BYTES = 1024u * 1024u;
        using Value                       = rapidjson::Value;
        using Writer                      = rapidjson::Writer<rapidjson::StringBuffer>;

        phy::PhysicsStatus Invalid(const char *message)
        {
            return {phy::PhysicsError::InvalidArgument, message};
        }
        bool Finite(Vector3 value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::abs(value.x) <= 1e6f &&
                   std::abs(value.y) <= 1e6f && std::abs(value.z) <= 1e6f;
        }
        bool Range(float value, float minimum, float maximum)
        {
            return std::isfinite(value) && value >= minimum && value <= maximum;
        }
        bool ValidMaterial(const RigMaterialSettings &value)
        {
            return Range(value.followCompliance, .000000001f, 1000.f) && Range(value.particleMass, .000001f, 100000.f) &&
                   Range(value.edgeCompliance, 0.f, 1000.f) && Range(value.volumeCompliance, 0.f, 1000.f) && Range(value.pressure, 0.f, 1000000.f) &&
                   Range(value.gravityFactor, 0.f, 100.f) && std::isfinite(value.linearDamping) && value.linearDamping >= 0.f &&
                   Range(value.vertexRadius, 0.f, 1.f) && Range(value.friction, 0.f, 1.f) && Range(value.restitution, 0.f, 1.f) &&
                   value.solverIterations >= 1 && value.solverIterations <= 64;
        }
        bool ParseId(std::string_view text, Uuid &out)
        {
            if (text.size() != 36)
                return false;
            for (size_t i = 0; i < text.size(); ++i) {
                if (i == 8 || i == 13 || i == 18 || i == 23) {
                    if (text[i] != '-')
                        return false;
                } else if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f') || (text[i] >= 'A' && text[i] <= 'F')))
                    return false;
            }
            const auto id = Uuid::CreateFromString(std::string(text));
            if (!id)
                return false;
            out = id;
            return true;
        }
        const Value *Field(const Value &object, const char *name)
        {
            if (!object.IsObject()) {
                return nullptr;
            }
            const auto found = object.FindMember(name);
            return found == object.MemberEnd() ? nullptr : &found->value;
        }
        bool Read(const Value &object, const char *name, std::string &out)
        {
            const auto *value = Field(object, name);
            if (!value || !value->IsString() || value->GetStringLength() > 4096) {
                return false;
            }
            out.assign(value->GetString(), value->GetStringLength());
            return out.find('\0') == std::string::npos;
        }
        bool Read(const Value &object, const char *name, uint32_t &out)
        {
            const auto *value = Field(object, name);
            if (!value || !value->IsUint()) {
                return false;
            }
            out = value->GetUint();
            return true;
        }
        bool Read(const Value &object, const char *name, float &out)
        {
            const auto *value = Field(object, name);
            if (!value || !value->IsNumber() || !std::isfinite(value->GetDouble()) ||
                std::abs(value->GetDouble()) > std::numeric_limits<float>::max()) {
                return false;
            }
            out = static_cast<float>(value->GetDouble());
            return true;
        }
        bool Read(const Value &object, const char *name, bool &out)
        {
            const auto *value = Field(object, name);
            if (!value || !value->IsBool()) {
                return false;
            }
            out = value->GetBool();
            return true;
        }
        bool Read(const Value &object, const char *name, Vector3 &out)
        {
            const auto *value = Field(object, name);
            if (!value || !value->IsArray() || value->Size() != 3) {
                return false;
            }
            for (uint32_t i = 0; i < 3; ++i) {
                const auto &element = (*value)[i];
                if (!element.IsNumber() || !std::isfinite(element.GetDouble()) || std::abs(element.GetDouble()) > 1e6) {
                    return false;
                }
                out[i] = static_cast<float>(element.GetDouble());
            }
            return true;
        }
        void Put(Writer &writer, const char *name, const std::string &value)
        {
            writer.Key(name);
            writer.String(value.data(), static_cast<rapidjson::SizeType>(value.size()));
        }
        void Put(Writer &writer, const char *name, uint32_t value)
        {
            writer.Key(name);
            writer.Uint(value);
        }
        void Put(Writer &writer, const char *name, float value)
        {
            writer.Key(name);
            writer.Double(value);
        }
        void Put(Writer &writer, const char *name, Vector3 value)
        {
            writer.Key(name);
            writer.StartArray();
            writer.Double(value.x);
            writer.Double(value.y);
            writer.Double(value.z);
            writer.EndArray();
        }
    } // namespace

    phy::PhysicsStatus ValidateSoftBodyRecipe(const SoftBodyRecipe &recipe)
    {
        const auto &settings  = recipe.settings;
        const auto &generator = settings.generator;
        if ((recipe.version != 1 && recipe.version != 2) || !recipe.source || recipe.sourceKey.empty() || recipe.sourceKey.size() > 4096 ||
            settings.bodyPrimitive.empty() || settings.bodyPrimitive.size() > 4096 || generator.maxControlPoints < MIN_RIG_CONTROL_POINTS ||
            generator.maxControlPoints > MAX_RIG_CONTROL_POINTS || !Range(generator.maxSurfaceDistance, .000001f, 1e3f) ||
            !Range(generator.maxVolumeExcess, 0.f, 10.f) || !Range(generator.minTetQuality, 0.f, 1.f) || !Finite(generator.cageCenter) ||
            !ValidMaterial(settings.material) || recipe.controls.size() > generator.maxControlPoints || settings.attachments.size() > 1024 ||
            static_cast<uint32_t>(generator.mode) >= static_cast<uint32_t>(RigGenerationMode::END) ||
            static_cast<uint32_t>(settings.normalPolicy) >= static_cast<uint32_t>(SoftBodyNormalPolicy::END) ||
            !Range(generator.bindingPositionScale, .000001f, 1000.f) || generator.tetrahedra.size() > 4096 || settings.materialSlots.size() > 1024 ||
            (generator.bindingPositionScale != 1.f && settings.normalPolicy != SoftBodyNormalPolicy::StandardSkinning)) {
            return Invalid("Invalid softbody recipe identity, budget or settings");
        }
        if (recipe.version == 1 &&
            (generator.mode != RigGenerationMode::AutoFit || generator.bindingPositionScale != 1.f || !generator.tetrahedra.empty() ||
             settings.normalPolicy != SoftBodyNormalPolicy::TetGradient || !settings.materialSlots.empty()))
            return Invalid("Supplied cages, displaced binding and normal policy require recipe version two");
        for (const auto &material : settings.materialSlots)
            if (!material || material == recipe.source)
                return Invalid("Material slots require nonempty material product identities");
        if (generator.mode == RigGenerationMode::SuppliedCenterFan) {
            if (recipe.controls.size() < 5 || generator.centerIndex >= recipe.controls.size() || generator.tetrahedra.empty() ||
                generator.fillControlBudget)
                return Invalid("Supplied center fan requires ordered controls, a center and fixed tetrahedra");
            for (const auto &tet : generator.tetrahedra) {
                std::array<uint32_t, 4> indices{tet.a, tet.b, tet.c, tet.d};
                if (std::find(indices.begin(), indices.end(), generator.centerIndex) == indices.end())
                    return Invalid("Supplied tetrahedron does not include its center");
                std::sort(indices.begin(), indices.end());
                if (indices.back() >= recipe.controls.size() || std::adjacent_find(indices.begin(), indices.end()) != indices.end())
                    return Invalid("Invalid supplied tetrahedron indices");
            }
        } else if (!generator.tetrahedra.empty() || generator.bindingPositionScale != 1.f) {
            return Invalid("Automatic fitting does not consume supplied tetrahedra or displaced binding positions");
        }
        std::unordered_set<std::string> ids;
        for (const auto &control : recipe.controls) {
            if (control.id.empty() || control.id.size() > 256 || control.id.starts_with("auto:") || !ids.insert(control.id).second ||
                !Finite(control.position) || static_cast<uint32_t>(control.mode) >= static_cast<uint32_t>(phy::SoftBodyAttachmentMode::END)) {
                return Invalid("Invalid or duplicate constrained control point");
            }
        }
        std::unordered_set<std::string> parts;
        for (const auto &attachment : settings.attachments) {
            if (attachment.primitiveKey.empty() || attachment.primitiveKey.size() > 4096 || attachment.primitiveKey == settings.bodyPrimitive ||
                !parts.insert(attachment.primitiveKey).second ||
                static_cast<uint32_t>(attachment.mode) >= static_cast<uint32_t>(AttachmentBindingMode::END) ||
                (attachment.mode == AttachmentBindingMode::Control
                     ? !ids.contains(attachment.controlId)
                     : (recipe.version < 2 || !attachment.controlId.empty() || !Finite(attachment.bindingAnchor) ||
                        settings.normalPolicy != SoftBodyNormalPolicy::StandardSkinning))) {
                return Invalid("Attachments must name a unique non-body primitive and an explicit authored control");
            }
        }
        return {};
    }

    phy::PhysicsStatus ParseSoftBodyRecipe(std::string_view text, SoftBodyRecipe &out)
    {
        if (text.empty() || text.size() > MAX_RECIPE_BYTES) {
            return Invalid("Softbody recipe exceeds the input size limit");
        }
        rapidjson::Document document;
        document.Parse<rapidjson::kParseIterativeFlag>(text.data(), text.size());
        SoftBodyRecipe next;
        std::string    source;
        if (document.HasParseError() || !Read(document, "version", next.version) || !Read(document, "source", source) || source.size() != 36 ||
            !Read(document, "sourceKey", next.sourceKey)) {
            return Invalid("Invalid softbody recipe header");
        }
        if (source[8] != '-' || source[13] != '-' || source[18] != '-' || source[23] != '-') {
            return Invalid("Invalid softbody source UUID");
        }
        for (size_t i = 0; i < source.size(); ++i) {
            if (i == 8 || i == 13 || i == 18 || i == 23) {
                continue;
            }
            const char value = source[i];
            if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F'))) {
                return Invalid("Invalid softbody source UUID digit");
            }
        }
        next.source = Uuid::CreateFromString(source);
        if (!next.source) {
            return Invalid("Softbody source UUID must not be empty");
        }
        const auto *settings = Field(document, "settings");
        const auto *controls = Field(document, "controls");
        if (!settings || !controls || !controls->IsArray() || controls->Size() > MAX_RIG_CONTROL_POINTS ||
            !Read(*settings, "bodyPrimitive", next.settings.bodyPrimitive)) {
            return Invalid("Invalid softbody settings or controls");
        }
        const auto *generator = Field(*settings, "generator");
        auto       &g         = next.settings.generator;
        if (!generator || !Read(*generator, "maxControlPoints", g.maxControlPoints) ||
            !Read(*generator, "maxSurfaceDistance", g.maxSurfaceDistance) || !Read(*generator, "maxVolumeExcess", g.maxVolumeExcess) ||
            !Read(*generator, "minTetQuality", g.minTetQuality) || !Read(*generator, "useExplicitCenter", g.useExplicitCenter) ||
            !Read(*generator, "cageCenter", g.cageCenter)) {
            return Invalid("Invalid generator parameters");
        }
        if (Field(*generator, "fillControlBudget") && !Read(*generator, "fillControlBudget", g.fillControlBudget))
            return Invalid("Invalid exact control budget setting");
        uint32_t generationMode = 0, normalPolicy = 0;
        if ((Field(*generator, "mode") && !Read(*generator, "mode", generationMode)) ||
            generationMode >= static_cast<uint32_t>(RigGenerationMode::END) ||
            (Field(*settings, "normalPolicy") && !Read(*settings, "normalPolicy", normalPolicy)) ||
            normalPolicy >= static_cast<uint32_t>(SoftBodyNormalPolicy::END) ||
            (Field(*generator, "centerIndex") && !Read(*generator, "centerIndex", g.centerIndex)) ||
            (Field(*generator, "bindingPositionScale") && !Read(*generator, "bindingPositionScale", g.bindingPositionScale)))
            return Invalid("Invalid generation mode, binding scale or normal policy");
        g.mode                     = static_cast<RigGenerationMode>(generationMode);
        next.settings.normalPolicy = static_cast<SoftBodyNormalPolicy>(normalPolicy);
        if (const auto *slots = Field(*settings, "materialSlots")) {
            if (!slots->IsArray() || slots->Size() > 1024)
                return Invalid("Invalid material slot array");
            for (const auto &value : slots->GetArray()) {
                Uuid id;
                if (!value.IsString() || !ParseId({value.GetString(), value.GetStringLength()}, id))
                    return Invalid("Invalid material slot identity");
                next.settings.materialSlots.push_back(id);
            }
        }
        if (const auto *tets = Field(*generator, "tetrahedra")) {
            if (!tets->IsArray() || tets->Size() > 4096)
                return Invalid("Invalid supplied tetrahedron array");
            for (const auto &tet : tets->GetArray()) {
                if (!tet.IsArray() || tet.Size() != 4 || !tet[0].IsUint() || !tet[1].IsUint() || !tet[2].IsUint() || !tet[3].IsUint())
                    return Invalid("Invalid supplied tetrahedron index");
                g.tetrahedra.push_back({tet[0].GetUint(), tet[1].GetUint(), tet[2].GetUint(), tet[3].GetUint()});
            }
        }
        const auto *material = Field(*settings, "material");
        auto       &m        = next.settings.material;
        if (!material || !Read(*material, "enableRootFollow", m.enableRootFollow) || !Read(*material, "followCompliance", m.followCompliance) ||
            !Read(*material, "particleMass", m.particleMass) || !Read(*material, "edgeCompliance", m.edgeCompliance) ||
            !Read(*material, "volumeCompliance", m.volumeCompliance) || !Read(*material, "pressure", m.pressure) ||
            !Read(*material, "gravityFactor", m.gravityFactor) || !Read(*material, "linearDamping", m.linearDamping) ||
            !Read(*material, "vertexRadius", m.vertexRadius) || !Read(*material, "friction", m.friction) ||
            !Read(*material, "restitution", m.restitution) || !Read(*material, "solverIterations", m.solverIterations)) {
            return Invalid("Invalid softbody material parameters");
        }
        for (const auto &value : controls->GetArray()) {
            ControlPoint point;
            uint32_t     mode = 0;
            if (!Read(value, "id", point.id) || !Read(value, "position", point.position) || !Read(value, "mode", mode) ||
                mode >= static_cast<uint32_t>(phy::SoftBodyAttachmentMode::END)) {
                return Invalid("Invalid control point or attachment mode");
            }
            point.mode = static_cast<phy::SoftBodyAttachmentMode>(mode);
            next.controls.push_back(std::move(point));
        }
        const auto *attachments = Field(*settings, "attachments");
        if (!attachments || !attachments->IsArray() || attachments->Size() > 1024) {
            return Invalid("Invalid rigid attachment list");
        }
        for (const auto &value : attachments->GetArray()) {
            Attachment attachment;
            uint32_t   mode = 0;
            if (!Read(value, "primitiveKey", attachment.primitiveKey) || !Read(value, "controlId", attachment.controlId)) {
                return Invalid("Invalid rigid attachment");
            }
            if ((Field(value, "mode") && !Read(value, "mode", mode)) || mode >= static_cast<uint32_t>(AttachmentBindingMode::END) ||
                (Field(value, "bindingAnchor") && !Read(value, "bindingAnchor", attachment.bindingAnchor)))
                return Invalid("Invalid constant binding anchor");
            attachment.mode = static_cast<AttachmentBindingMode>(mode);
            if (attachment.mode == AttachmentBindingMode::BindingAnchor && !Field(value, "bindingAnchor"))
                return Invalid("Constant binding attachment requires an explicit anchor");
            next.settings.attachments.push_back(std::move(attachment));
        }
        if (const auto status = ValidateSoftBodyRecipe(next); !status) {
            return status;
        }
        out = std::move(next);
        return {};
    }

    phy::PhysicsStatus WriteSoftBodyRecipe(const SoftBodyRecipe &recipe, std::string &out)
    {
        if (const auto status = ValidateSoftBodyRecipe(recipe); !status) {
            return status;
        }
        rapidjson::StringBuffer buffer;
        Writer                  writer(buffer);
        writer.StartObject();
        Put(writer, "version", recipe.version);
        Put(writer, "source", recipe.source.ToString());
        Put(writer, "sourceKey", recipe.sourceKey);
        writer.Key("settings");
        writer.StartObject();
        Put(writer, "bodyPrimitive", recipe.settings.bodyPrimitive);
        if (recipe.version >= 2) {
            Put(writer, "normalPolicy", static_cast<uint32_t>(recipe.settings.normalPolicy));
            writer.Key("materialSlots");
            writer.StartArray();
            for (const auto &id : recipe.settings.materialSlots) {
                const auto value = id.ToString();
                writer.String(value.data(), static_cast<rapidjson::SizeType>(value.size()));
            }
            writer.EndArray();
        }
        writer.Key("generator");
        writer.StartObject();
        const auto &g = recipe.settings.generator;
        Put(writer, "maxControlPoints", g.maxControlPoints);
        writer.Key("fillControlBudget");
        writer.Bool(g.fillControlBudget);
        Put(writer, "maxSurfaceDistance", g.maxSurfaceDistance);
        Put(writer, "maxVolumeExcess", g.maxVolumeExcess);
        Put(writer, "minTetQuality", g.minTetQuality);
        writer.Key("useExplicitCenter");
        writer.Bool(g.useExplicitCenter);
        Put(writer, "cageCenter", g.cageCenter);
        if (recipe.version >= 2) {
            Put(writer, "mode", static_cast<uint32_t>(g.mode));
            Put(writer, "centerIndex", g.centerIndex);
            Put(writer, "bindingPositionScale", g.bindingPositionScale);
            writer.Key("tetrahedra");
            writer.StartArray();
            for (const auto &tet : g.tetrahedra) {
                writer.StartArray();
                for (const auto index : {tet.a, tet.b, tet.c, tet.d})
                    writer.Uint(index);
                writer.EndArray();
            }
            writer.EndArray();
        }
        writer.EndObject();
        writer.Key("material");
        writer.StartObject();
        const auto &m = recipe.settings.material;
        writer.Key("enableRootFollow");
        writer.Bool(m.enableRootFollow);
        Put(writer, "followCompliance", m.followCompliance);
        Put(writer, "particleMass", m.particleMass);
        Put(writer, "edgeCompliance", m.edgeCompliance);
        Put(writer, "volumeCompliance", m.volumeCompliance);
        Put(writer, "pressure", m.pressure);
        Put(writer, "gravityFactor", m.gravityFactor);
        Put(writer, "linearDamping", m.linearDamping);
        Put(writer, "vertexRadius", m.vertexRadius);
        Put(writer, "friction", m.friction);
        Put(writer, "restitution", m.restitution);
        Put(writer, "solverIterations", m.solverIterations);
        writer.EndObject();
        writer.Key("attachments");
        writer.StartArray();
        for (const auto &attachment : recipe.settings.attachments) {
            writer.StartObject();
            Put(writer, "primitiveKey", attachment.primitiveKey);
            Put(writer, "controlId", attachment.controlId);
            if (recipe.version >= 2) {
                Put(writer, "mode", static_cast<uint32_t>(attachment.mode));
                Put(writer, "bindingAnchor", attachment.bindingAnchor);
            }
            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
        writer.Key("controls");
        writer.StartArray();
        for (const auto &point : recipe.controls) {
            writer.StartObject();
            Put(writer, "id", point.id);
            Put(writer, "position", point.position);
            Put(writer, "mode", static_cast<uint32_t>(point.mode));
            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
        if (buffer.GetSize() > MAX_RECIPE_BYTES) {
            return Invalid("Serialized softbody recipe exceeds its size limit");
        }
        out.assign(buffer.GetString(), buffer.GetSize());
        return {};
    }

    phy::PhysicsStatus ConfigureRigPhysics(const RigMaterialSettings &settings, const phy::SoftBodyMesh &mesh, phy::SoftBodyDesc &out)
    {
        if (!ValidMaterial(settings)) {
            return Invalid("Invalid softbody material settings");
        }
        if (settings.pressure != 0)
            return {phy::PhysicsError::Unsupported, "CPU softbody recipes use volume compliance; gas pressure is unsupported"};
        phy::SoftBodyDesc next;
        next.mesh             = mesh;
        next.pressure         = settings.pressure;
        next.gravityFactor    = settings.gravityFactor;
        next.linearDamping    = settings.linearDamping;
        next.vertexRadius     = settings.vertexRadius;
        next.friction         = settings.friction;
        next.restitution      = settings.restitution;
        next.solverIterations = settings.solverIterations;
        for (uint32_t index = 0; index < next.mesh.vertices.size(); ++index) {
            auto &point       = next.mesh.vertices[index];
            point.inverseMass = point.mode == phy::SoftBodyAttachmentMode::Dynamic ? 1.f / settings.particleMass : 0.f;
            point.velocity    = VEC3_ZERO;
            if (settings.enableRootFollow && point.mode == phy::SoftBodyAttachmentMode::Dynamic) {
                next.rootFollow.push_back({index, settings.followCompliance});
            }
        }
        for (auto &edge : next.mesh.edges) {
            edge.compliance = settings.edgeCompliance;
        }
        for (auto &tet : next.mesh.tetrahedra) {
            tet.compliance = settings.volumeCompliance;
        }
        if (const auto status = phy::ValidateSoftBodyDesc(next); !status) {
            return status;
        }
        out = std::move(next);
        return {};
    }

    void RegisterSoftBodyRecipeTypes()
    {
        auto *context = SerializationContext::Get();
        context->Register<RigGenerationMode>("SoftBodyRigGenerationMode")
            .Enum(RigGenerationMode::AutoFit, "Auto fit")
            .Enum(RigGenerationMode::SuppliedCenterFan, "Supplied center fan");
        context->Register<AttachmentBindingMode>("SoftBodyAttachmentBindingMode")
            .Enum(AttachmentBindingMode::Control, "Control")
            .Enum(AttachmentBindingMode::BindingAnchor, "Constant binding anchor");
        context->Register<SoftBodyNormalPolicy>("SoftBodyNormalPolicy")
            .Enum(SoftBodyNormalPolicy::TetGradient, "Tetrahedral gradient")
            .Enum(SoftBodyNormalPolicy::StandardSkinning, "Standard skinning");
        context->Register<SuppliedTetrahedron>("SoftBodySuppliedTetrahedron")
            .Member<&SuppliedTetrahedron::a>("a")
            .Member<&SuppliedTetrahedron::b>("b")
            .Member<&SuppliedTetrahedron::c>("c")
            .Member<&SuppliedTetrahedron::d>("d");
        context->Register<RigGeneratorSettings>("RigGeneratorSettings")
            .Member<&RigGeneratorSettings::mode>("mode")
            .Member<&RigGeneratorSettings::centerIndex>("centerIndex")
            .Member<&RigGeneratorSettings::tetrahedra>("tetrahedra")
            .Member<&RigGeneratorSettings::bindingPositionScale>("bindingPositionScale")
            .Member<&RigGeneratorSettings::maxControlPoints>("maxControlPoints")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Maximum control points")))
            .Property(CommonPropertyKey::RANGE_MIN, Any(MIN_RIG_CONTROL_POINTS))
            .Property(CommonPropertyKey::RANGE_MAX, Any(MAX_RIG_CONTROL_POINTS))
            .Member<&RigGeneratorSettings::fillControlBudget>("fillControlBudget")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Fill control budget")))
            .Member<&RigGeneratorSettings::maxSurfaceDistance>("maxSurfaceDistance")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Maximum cage gap (m)")))
            .Member<&RigGeneratorSettings::maxVolumeExcess>("maxVolumeExcess")
            .Member<&RigGeneratorSettings::minTetQuality>("minTetQuality")
            .Property(CommonPropertyKey::EDITOR_HINT, Any(std::string("scientific")))
            .Member<&RigGeneratorSettings::useExplicitCenter>("useExplicitCenter")
            .Member<&RigGeneratorSettings::cageCenter>("cageCenter");
        context->Register<RigMaterialSettings>("RigMaterialSettings")
            .Member<&RigMaterialSettings::enableRootFollow>("enableRootFollow")
            .Member<&RigMaterialSettings::followCompliance>("followCompliance")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Root follow compliance (s^2/kg)")))
            .Member<&RigMaterialSettings::particleMass>("particleMass")
            .Member<&RigMaterialSettings::edgeCompliance>("edgeCompliance")
            .Property(CommonPropertyKey::EDITOR_HINT, Any(std::string("scientific")))
            .Member<&RigMaterialSettings::volumeCompliance>("volumeCompliance")
            .Property(CommonPropertyKey::EDITOR_HINT, Any(std::string("scientific")))
            .Property(CommonPropertyKey::LABEL, Any(std::string("Volume compliance (m^4 s^2/kg)")))
            .Member<&RigMaterialSettings::gravityFactor>("gravityFactor")
            .Member<&RigMaterialSettings::linearDamping>("linearDamping")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Velocity damping rate (s^-1)")))
            .Member<&RigMaterialSettings::vertexRadius>("vertexRadius")
            .Member<&RigMaterialSettings::friction>("friction")
            .Member<&RigMaterialSettings::restitution>("restitution")
            .Member<&RigMaterialSettings::solverIterations>("solverIterations");
        context->Register<Attachment>("SoftBodyRigidAttachment")
            .Member<&Attachment::primitiveKey>("primitiveKey")
            .Member<&Attachment::controlId>("controlId")
            .Member<&Attachment::mode>("mode")
            .Member<&Attachment::bindingAnchor>("bindingAnchor");
        context->Register<RigAuthoringSettings>("SoftBodyRigAuthoringSettings")
            .Member<&RigAuthoringSettings::bodyPrimitive>("bodyPrimitive")
            .Member<&RigAuthoringSettings::generator>("generator")
            .Member<&RigAuthoringSettings::material>("material")
            .Member<&RigAuthoringSettings::normalPolicy>("normalPolicy")
            .Member<&RigAuthoringSettings::materialSlots>("materialSlots")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Material product slots")))
            .Member<&RigAuthoringSettings::attachments>("attachments");
    }
} // namespace sky::softbody
