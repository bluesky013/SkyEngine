#include <particle/ParticleSchema.h>

#include <algorithm>
#include <cmath>

namespace sky::particle {
    namespace {
        constexpr uint32_t Kind(ParticleValueKind kind)
        {
            return 1u << static_cast<uint32_t>(kind);
        }
        constexpr uint32_t                CONSTANT     = Kind(ParticleValueKind::CONSTANT);
        constexpr uint32_t                RANDOM       = Kind(ParticleValueKind::RANDOM);
        constexpr uint32_t                CURVE        = Kind(ParticleValueKind::CURVE);
        constexpr ParticleParameterSchema RATE[]       = {{"rate", ParticleParameterShape::SCALAR, CONSTANT}};
        constexpr ParticleParameterSchema BURST[]      = {{"interval", ParticleParameterShape::SCALAR, CONSTANT},
                                                          {"count", ParticleParameterShape::SCALAR, CONSTANT}};
        constexpr ParticleParameterSchema INITIALIZE[] = {{"lifetime", ParticleParameterShape::SCALAR, CONSTANT | RANDOM},
                                                          {"size", ParticleParameterShape::SCALAR, CONSTANT | RANDOM},
                                                          {"rotation", ParticleParameterShape::SCALAR, CONSTANT | RANDOM},
                                                          {"position", ParticleParameterShape::VECTOR, CONSTANT},
                                                          {"velocity", ParticleParameterShape::VECTOR, CONSTANT}};
        constexpr ParticleParameterSchema GRAVITY[]    = {{"gravity", ParticleParameterShape::VECTOR, CONSTANT}};
        constexpr ParticleParameterSchema DRAG[]       = {{"drag", ParticleParameterShape::SCALAR, CONSTANT}};
        constexpr ParticleParameterSchema COLOR[]      = {{"color", ParticleParameterShape::GRADIENT, Kind(ParticleValueKind::GRADIENT)}};
        constexpr ParticleParameterSchema SIZE[]       = {{"size", ParticleParameterShape::SCALAR, CONSTANT | CURVE}};
        constexpr ParticleParameterSchema ROTATION[]   = {{"rotation", ParticleParameterShape::SCALAR, CONSTANT | CURVE}};
        constexpr ParticleParameterSchema SUB_UV[]     = {{"frame", ParticleParameterShape::SCALAR, CONSTANT | CURVE},
                                                          {"columns", ParticleParameterShape::SCALAR, CONSTANT},
                                                          {"rows", ParticleParameterShape::SCALAR, CONSTANT}};
        constexpr ParticleParameterSchema KILL[]       = {{"min", ParticleParameterShape::SCALAR, CONSTANT},
                                                          {"max", ParticleParameterShape::SCALAR, CONSTANT}};
        const ParticleModuleSchema        SCHEMAS[]    = {{ParticleModuleType::SPAWN_RATE, "SPAWN_RATE", "Spawn Rate", RATE},
                                                          {ParticleModuleType::BURST, "BURST", "Burst", BURST},
                                                          {ParticleModuleType::INITIALIZE, "INITIALIZE", "Initialize", INITIALIZE},
                                                          {ParticleModuleType::GRAVITY, "GRAVITY", "Gravity", GRAVITY},
                                                          {ParticleModuleType::DRAG, "DRAG", "Drag", DRAG},
                                                          {ParticleModuleType::COLOR_OVER_LIFE, "COLOR_OVER_LIFE", "Color Over Life", COLOR},
                                                          {ParticleModuleType::SIZE_OVER_LIFE, "SIZE_OVER_LIFE", "Size Over Life", SIZE},
                                                          {ParticleModuleType::ROTATION_OVER_LIFE, "ROTATION_OVER_LIFE", "Rotation Over Life", ROTATION},
                                                          {ParticleModuleType::SUB_UV, "SUB_UV", "Sub UV", SUB_UV},
                                                          {ParticleModuleType::KILL_VOLUME, "KILL_VOLUME", "Kill Volume", KILL}};
        bool                              Finite(const Vector3 &value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }
        bool Fail(std::string &error, const char *message)
        {
            error = message;
            return false;
        }
    } // namespace
    std::span<const ParticleModuleSchema> ParticleModuleSchemas()
    {
        return SCHEMAS;
    }
    const ParticleModuleSchema *FindParticleModuleSchema(ParticleModuleType type)
    {
        for (const auto &schema : SCHEMAS)
            if (schema.type == type)
                return &schema;
        return nullptr;
    }
    ParticleModuleType ParseParticleModuleType(std::string_view token)
    {
        for (const auto &schema : SCHEMAS)
            if (schema.token == token)
                return schema.type;
        return ParticleModuleType::END;
    }
    bool ValidateParticleValue(const ParticleValue &value, std::string &error)
    {
        if (value.kind >= ParticleValueKind::END || !IsValidAttribute(value.attribute) || !std::isfinite(value.constant) || !Finite(value.vector) ||
            !std::isfinite(value.range.min) || !std::isfinite(value.range.max) || value.range.min > value.range.max)
            return Fail(error, "Particle value is invalid");
        if (value.curve.keys.size() > MAX_PARTICLE_KEYS || value.gradient.keys.size() > MAX_PARTICLE_KEYS)
            return Fail(error, "Particle value has too many keys");
        float previous = -1.f;
        for (const auto &key : value.curve.keys) {
            if (!std::isfinite(key.time) || key.time < 0.f || key.time > 1.f || key.time <= previous || !Finite(key.value))
                return Fail(error, "Particle curve keys must be finite and strictly ordered in [0, 1]");
            previous = key.time;
        }
        previous = -1.f;
        for (const auto &key : value.gradient.keys) {
            if (!std::isfinite(key.time) || key.time < 0.f || key.time > 1.f || key.time <= previous ||
                !std::all_of(std::begin(key.color.v), std::end(key.color.v), [](float channel) { return std::isfinite(channel); }))
                return Fail(error, "Particle gradient keys must be finite and strictly ordered in [0, 1]");
            previous = key.time;
        }
        if ((value.kind == ParticleValueKind::CURVE && value.curve.keys.empty()) ||
            (value.kind == ParticleValueKind::GRADIENT && value.gradient.keys.empty()))
            return Fail(error, "Particle curve or gradient requires at least one key");
        return true;
    }
    bool ValidateParticleEffect(const ParticleEffectDesc &effect, std::string &error)
    {
        error.clear();
        if (effect.version != 1 || effect.emitters.empty() || effect.emitters.size() > MAX_PARTICLE_EMITTERS || !effect.fixedBounds.Valid() ||
            effect.userParameters.size() > 256)
            return Fail(error, "Particle effect version, bounds or emitter count is invalid");
        for (const auto &parameter : effect.userParameters)
            if (parameter.name.GetStr().empty() || parameter.name.GetStr().size() > 256 || !ValidateParticleValue(parameter.value, error))
                return Fail(error, "Particle user parameter is invalid");
        for (const auto &emitter : effect.emitters) {
            if (!IsValidSimTarget(emitter.simTarget) || emitter.space >= ParticleSimSpace::END || !emitter.capacity ||
                emitter.capacity > MAX_PARTICLE_CAPACITY || !emitter.bounds.Valid() || emitter.name.GetStr().size() > 256 ||
                emitter.modules.size() > MAX_PARTICLE_MODULES || emitter.renderers.size() > MAX_PARTICLE_RENDERERS)
                return Fail(error, "Particle emitter descriptor is invalid");
            if (!std::isfinite(emitter.spawn.rate) || emitter.spawn.rate < 0.f || !std::isfinite(emitter.spawn.burstInterval) ||
                emitter.spawn.burstInterval < 0.f || !std::isfinite(emitter.spawn.warmupSeconds) || emitter.spawn.warmupSeconds < 0.f ||
                emitter.spawn.warmupSeconds > MAX_PARTICLE_WARMUP_SECONDS)
                return Fail(error, "Particle spawn settings are invalid");
            for (const auto &renderer : emitter.renderers)
                if (!IsValidRendererType(renderer.type) || renderer.blend >= ParticleBlendMode::END)
                    return Fail(error, "Particle renderer descriptor is invalid");
            uint32_t rateModules = 0, burstModules = 0;
            for (const auto &module : emitter.modules) {
                if ((module.type == ParticleModuleType::SPAWN_RATE && ++rateModules > 1) ||
                    (module.type == ParticleModuleType::BURST && ++burstModules > 1))
                    return Fail(error, "Only one spawn-rate and burst configuration module is supported");
                const auto *schema = FindParticleModuleSchema(module.type);
                if (!schema || module.name.GetStr().size() > 256 || module.parameters.size() > schema->parameters.size())
                    return Fail(error, "Particle module descriptor is invalid");
                for (size_t i = 0; i < module.parameters.size(); ++i) {
                    const auto &parameter     = module.parameters[i];
                    const auto  parameterName = parameter.name.GetStr();
                    const auto  found         = std::find_if(schema->parameters.begin(), schema->parameters.end(),
                                                             [&](const auto &entry) { return entry.name == parameterName; });
                    if (found == schema->parameters.end() || !ValidateParticleValue(parameter.value, error) ||
                        !(found->kinds & Kind(parameter.value.kind)))
                        return Fail(error, "Particle module parameter name or value kind is unsupported");
                    const bool vectorValue = std::any_of(std::begin(parameter.value.vector.v), std::end(parameter.value.vector.v),
                                                         [](float channel) { return channel != 0.f; });
                    if ((found->shape == ParticleParameterShape::SCALAR && vectorValue) ||
                        (found->shape == ParticleParameterShape::VECTOR && !vectorValue && parameter.value.constant != 0.f))
                        return Fail(error, "Particle module parameter shape does not match its schema");
                    for (size_t j = 0; j < i; ++j)
                        if (module.parameters[j].name == parameter.name)
                            return Fail(error, "Particle module parameter is duplicated");
                    const auto scalar = parameter.value.kind == ParticleValueKind::RANDOM ? parameter.value.range.min : parameter.value.constant;
                    if ((parameterName == "lifetime" && scalar <= 0.f) ||
                        ((parameterName == "size" || parameterName == "rate" || parameterName == "interval" || parameterName == "count" ||
                          parameterName == "drag") &&
                         scalar < 0.f))
                        return Fail(error, "Particle module parameter is outside its valid range");
                    if ((parameterName == "count" || parameterName == "columns" || parameterName == "rows") &&
                        (scalar < (parameterName == "count" ? 0.f : 1.f) ||
                         scalar > float(parameterName == "count" ? MAX_PARTICLE_CAPACITY : MAX_PARTICLE_SUB_UV_DIMENSION) ||
                         std::floor(scalar) != scalar))
                        return Fail(error, "Particle count and grid parameters must be bounded integers");
                }
            }
        }
        return true;
    }
} // namespace sky::particle
