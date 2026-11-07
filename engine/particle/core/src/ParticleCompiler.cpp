#include <particle/ParticleProgram.h>

#include <bit>
#include <core/hash/Fnv1a.h>
#include <particle/ParticleSchema.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace sky::particle {
    namespace {
        uint64_t HashBytes(uint64_t hash, const void *data, size_t size)
        {
            const auto *bytes = static_cast<const uint8_t *>(data);
            for (size_t i = 0; i < size; ++i) {
                hash ^= bytes[i];
                hash *= 1099511628211ull;
            }
            return hash;
        }

        uint64_t HashString(uint64_t hash, std::string_view value)
        {
            return HashBytes(hash, value.data(), value.size());
        }

        uint32_t AttributeSize(ParticleAttribute attribute)
        {
            switch (attribute) {
            case ParticleAttribute::POSITION:
            case ParticleAttribute::VELOCITY: return sizeof(float) * 3;
            case ParticleAttribute::COLOR: return sizeof(float) * 4;
            default: return sizeof(float);
            }
        }

        const ParticleValue *FindParameter(const ParticleModuleDesc &module, const char *name)
        {
            const Name key(name);
            const auto found = std::find_if(module.parameters.begin(), module.parameters.end(), [&](const auto &entry) { return entry.name == key; });
            return found == module.parameters.end() ? nullptr : &found->value;
        }

        float ScalarOr(const ParticleModuleDesc &module, const char *name, float fallback)
        {
            const auto *value = FindParameter(module, name);
            return value ? (value->kind == ParticleValueKind::RANDOM ? value->range.min : value->constant) : fallback;
        }

        Vector3 VectorOr(const ParticleModuleDesc &module, const char *name, Vector3 fallback)
        {
            const auto *value = FindParameter(module, name);
            return value ? value->vector : fallback;
        }

        void CompileModules(const std::vector<ParticleModuleDesc> &source,
                            std::vector<ParticleCompiledModule>   &target,
                            ParticleProgramFlags                  &flags,
                            ParticleSpawnDesc                     &spawn)
        {
            for (const auto &module : source) {
                if (!IsValidModuleType(module.type))
                    continue;
                ParticleCompiledModule compiled;
                compiled.type = module.type;
                switch (module.type) {
                case ParticleModuleType::SPAWN_RATE: spawn.rate = ScalarOr(module, "rate", spawn.rate); break;
                case ParticleModuleType::BURST:
                    spawn.burstInterval = ScalarOr(module, "interval", spawn.burstInterval);
                    spawn.burstCount    = static_cast<uint32_t>(std::max(0.f, ScalarOr(module, "count", float(spawn.burstCount))));
                    flags               = flags | ParticleProgramFlags::BURST;
                    target.push_back(compiled);
                    break;
                case ParticleModuleType::INITIALIZE: break;
                case ParticleModuleType::GRAVITY: {
                    const auto gravity = VectorOr(module, "gravity", Vector3{0.f, -9.81f, 0.f});
                    compiled.values    = {gravity.x, gravity.y, gravity.z, 0.f};
                    flags              = flags | ParticleProgramFlags::GRAVITY;
                    target.push_back(compiled);
                    break;
                }
                case ParticleModuleType::DRAG:
                    compiled.values[0] = ScalarOr(module, "drag", 0.f);
                    flags              = flags | ParticleProgramFlags::DRAG;
                    target.push_back(compiled);
                    break;
                case ParticleModuleType::COLOR_OVER_LIFE: {
                    const auto *value = FindParameter(module, "color");
                    if (value)
                        compiled.gradient = value->gradient;
                    flags = flags | ParticleProgramFlags::COLOR_OVER_LIFE;
                    target.push_back(compiled);
                    break;
                }
                case ParticleModuleType::SIZE_OVER_LIFE: {
                    const auto *value = FindParameter(module, "size");
                    if (value)
                        compiled.curve = value->kind == ParticleValueKind::CURVE ? value->curve : ParticleCurve{{{0.f, {value->constant, 0.f, 0.f}}}};
                    flags = flags | ParticleProgramFlags::SIZE_OVER_LIFE;
                    target.push_back(compiled);
                    break;
                }
                case ParticleModuleType::ROTATION_OVER_LIFE: {
                    const auto *value = FindParameter(module, "rotation");
                    if (value)
                        compiled.curve = value->kind == ParticleValueKind::CURVE ? value->curve : ParticleCurve{{{0.f, {value->constant, 0.f, 0.f}}}};
                    flags = flags | ParticleProgramFlags::ROTATION_OVER_LIFE;
                    target.push_back(compiled);
                    break;
                }
                case ParticleModuleType::SUB_UV: {
                    const auto *value = FindParameter(module, "frame");
                    if (value)
                        compiled.curve = value->kind == ParticleValueKind::CURVE ? value->curve : ParticleCurve{{{0.f, {value->constant, 0.f, 0.f}}}};
                    compiled.values[0] = std::max(1.f, ScalarOr(module, "columns", 1.f));
                    compiled.values[1] = std::max(1.f, ScalarOr(module, "rows", 1.f));
                    flags              = flags | ParticleProgramFlags::SUB_UV;
                    target.push_back(compiled);
                    break;
                }
                case ParticleModuleType::KILL_VOLUME:
                    compiled.values = {ScalarOr(module, "min", 0.f), ScalarOr(module, "max", 0.f), 0.f, 0.f};
                    flags           = flags | ParticleProgramFlags::KILL_VOLUME;
                    target.push_back(compiled);
                    break;
                default: break;
                }
            }
        }

    } // namespace

    uint64_t HashParticleProgram(const ParticleProgram &program)
    {
        uint64_t   hash    = FNVOffsetBias64;
        const auto integer = [&](uint64_t value) {
            for (uint32_t byte = 0; byte < 8; ++byte) {
                const uint8_t bits = static_cast<uint8_t>(value >> (byte * 8));
                hash               = HashBytes(hash, &bits, 1);
            }
        };
        const auto scalar = [&](float value) { integer(std::bit_cast<uint32_t>(value == 0.f ? 0.f : value)); };
        const auto vector = [&](const Vector3 &value) {
            scalar(value.x);
            scalar(value.y);
            scalar(value.z);
        };
        const auto string = [&](std::string_view value) {
            integer(value.size());
            hash = HashString(hash, value);
        };
        integer(static_cast<uint32_t>(program.flags));
        integer(program.capacity);
        integer(static_cast<uint32_t>(program.space));
        integer(program.layout.version);
        integer(program.layout.stride);
        for (const auto offset : program.layout.offsets)
            integer(offset);
        vector(program.bounds.min);
        vector(program.bounds.max);
        scalar(program.spawn.rate);
        scalar(program.spawn.burstInterval);
        integer(program.spawn.burstCount);
        scalar(program.spawn.warmupSeconds);
        const auto modules = [&](const auto &items) {
            integer(items.size());
            for (const auto &module : items) {
                integer(static_cast<uint32_t>(module.type));
                integer(static_cast<uint32_t>(module.attribute));
                for (const auto value : module.values)
                    scalar(value);
                integer(module.randomMask);
                for (const auto value : module.randomMaximum)
                    scalar(value);
                vector(module.position);
                vector(module.velocity);
                integer(module.curve.keys.size());
                for (const auto &key : module.curve.keys) {
                    scalar(key.time);
                    vector(key.value);
                }
                integer(module.gradient.keys.size());
                for (const auto &key : module.gradient.keys) {
                    scalar(key.time);
                    for (const auto channel : key.color.v)
                        scalar(channel);
                }
            }
        };
        modules(program.spawnModules);
        modules(program.updateModules);
        integer(program.renderers.size());
        for (const auto &renderer : program.renderers) {
            integer(static_cast<uint32_t>(renderer.type));
            integer(static_cast<uint32_t>(renderer.blend));
            integer(renderer.lit);
            integer(renderer.soft);
            string(renderer.texture.ToString());
        }
        string(program.emitter.GetStr());
        return hash;
    }

    bool ValidateParticleProgram(const ParticleProgram &program, std::string &error)
    {
        ParticleEffectDesc  effect;
        ParticleEmitterDesc emitter;
        emitter.capacity = program.capacity;
        emitter.space    = program.space;
        emitter.spawn    = program.spawn;
        emitter.bounds   = program.bounds;
        for (const auto &renderer : program.renderers)
            emitter.renderers.push_back({renderer.type, renderer.blend, renderer.lit, renderer.soft, renderer.texture});
        effect.emitters.push_back(emitter);
        if (!ValidateParticleEffect(effect, error) || program.space == ParticleSimSpace::CUSTOM ||
            program.spawnModules.size() > MAX_PARTICLE_MODULES || program.updateModules.size() > MAX_PARTICLE_MODULES ||
            (static_cast<uint32_t>(program.flags) & ~0xffu) != 0 || program.layout.version != 1)
            return error = "Particle compiled program is invalid", false;
        uint32_t stride = 0;
        for (uint32_t attribute = 0; attribute < PARTICLE_ATTRIBUTE_COUNT; ++attribute) {
            if (program.layout.offsets[attribute] != stride)
                return error = "Particle compiled layout is invalid", false;
            stride += AttributeSize(static_cast<ParticleAttribute>(attribute));
        }
        if (program.layout.stride != stride)
            return error = "Particle compiled stride is invalid", false;
        const auto validateModules = [&](const auto &modules, bool spawn) {
            for (const auto &module : modules) {
                if (!IsValidModuleType(module.type) || !IsValidAttribute(module.attribute) || module.randomMask > 7u ||
                    (spawn != (module.type == ParticleModuleType::INITIALIZE)) ||
                    !std::all_of(module.values.begin(), module.values.end(), [](float value) { return std::isfinite(value); }))
                    return false;
                ParticleValue value;
                value.curve    = module.curve;
                value.gradient = module.gradient;
                value.vector   = module.position;
                if (!ValidateParticleValue(value, error))
                    return false;
                value.vector = module.velocity;
                if (!ValidateParticleValue(value, error))
                    return false;
                for (size_t i = 0; i < module.randomMaximum.size(); ++i)
                    if (!std::isfinite(module.randomMaximum[i]) || ((module.randomMask & (1u << i)) && module.randomMaximum[i] < module.values[i]))
                        return false;
                if (spawn && (module.values[0] <= 0.f || module.values[1] < 0.f))
                    return false;
                if (module.type == ParticleModuleType::SUB_UV &&
                    (module.values[0] < 1.f || module.values[1] < 1.f || module.values[0] > float(MAX_PARTICLE_SUB_UV_DIMENSION) ||
                     module.values[1] > float(MAX_PARTICLE_SUB_UV_DIMENSION) || std::floor(module.values[0]) != module.values[0] ||
                     std::floor(module.values[1]) != module.values[1]))
                    return false;
                if (module.type == ParticleModuleType::DRAG && module.values[0] < 0.f)
                    return false;
                if (module.type == ParticleModuleType::KILL_VOLUME && module.values[0] > module.values[1])
                    return false;
            }
            return true;
        };
        if (!validateModules(program.spawnModules, true) || !validateModules(program.updateModules, false))
            return error = "Particle compiled module is invalid", false;
        return true;
    }

    ParticleCompileResult CompileParticleEffect(const ParticleEffectDesc &effect)
    {
        ParticleCompileResult result;
        std::string           validation;
        if (!ValidateParticleEffect(effect, validation)) {
            result.diagnostics.push_back({0, validation});
            return result;
        }
        for (uint32_t emitterIndex = 0; emitterIndex < effect.emitters.size(); ++emitterIndex) {
            const auto &emitter = effect.emitters[emitterIndex];
            if (emitter.simTarget != ParticleSimTarget::CPU || emitter.space == ParticleSimSpace::CUSTOM) {
                result.diagnostics.push_back({emitterIndex, "GPU simulation and custom simulation space are not implemented"});
                continue;
            }
            if (std::any_of(emitter.renderers.begin(), emitter.renderers.end(),
                            [](const auto &renderer) { return renderer.type == ParticleRendererType::MESH; })) {
                result.diagnostics.push_back({emitterIndex, "Mesh particle rendering is not implemented"});
                continue;
            }
            if (std::any_of(emitter.renderers.begin(), emitter.renderers.end(), [](const auto &renderer) { return renderer.lit; })) {
                result.diagnostics.push_back({emitterIndex, "Lit particle rendering is not implemented"});
                continue;
            }
            if (std::any_of(emitter.renderers.begin(), emitter.renderers.end(), [](const auto &renderer) { return renderer.soft; })) {
                result.diagnostics.push_back({emitterIndex, "Soft particle rendering is not implemented"});
                continue;
            }
            const bool boundParameter = std::any_of(emitter.modules.begin(), emitter.modules.end(), [](const auto &module) {
                return std::any_of(module.parameters.begin(), module.parameters.end(), [](const auto &parameter) {
                    return parameter.value.kind == ParticleValueKind::PARAMETER || parameter.value.kind == ParticleValueKind::ATTRIBUTE;
                });
            });
            if (boundParameter) {
                result.diagnostics.push_back({emitterIndex, "User parameter and attribute bindings are not implemented"});
                continue;
            }
            ParticleProgram program;
            program.emitter       = emitter.name;
            program.space         = emitter.space;
            program.capacity      = emitter.capacity;
            program.bounds        = emitter.bounds.Valid() ? emitter.bounds : effect.fixedBounds;
            program.spawn         = emitter.spawn;
            program.layout.stride = 0;
            for (uint32_t attribute = 0; attribute < PARTICLE_ATTRIBUTE_COUNT; ++attribute) {
                const auto size                   = AttributeSize(static_cast<ParticleAttribute>(attribute));
                program.layout.offsets[attribute] = program.layout.stride;
                program.layout.stride += size;
            }
            for (const auto &renderer : emitter.renderers)
                program.renderers.push_back({renderer.type, renderer.blend, renderer.lit, renderer.soft, renderer.texture});
            CompileModules(emitter.modules, program.updateModules, program.flags, program.spawn);
            for (const auto &module : emitter.modules) {
                if (module.type == ParticleModuleType::INITIALIZE) {
                    ParticleCompiledModule compiled;
                    compiled.type       = module.type;
                    compiled.values[0]  = ScalarOr(module, "lifetime", 1.f);
                    compiled.values[1]  = ScalarOr(module, "size", 1.f);
                    compiled.values[2]  = ScalarOr(module, "rotation", 0.f);
                    const char *names[] = {"lifetime", "size", "rotation"};
                    for (uint32_t i = 0; i < 3; ++i) {
                        const auto *value = FindParameter(module, names[i]);
                        if (value && value->kind == ParticleValueKind::RANDOM) {
                            compiled.randomMask |= 1u << i;
                            compiled.randomMaximum[i] = value->range.max;
                        }
                    }
                    compiled.position = VectorOr(module, "position", {});
                    compiled.velocity = VectorOr(module, "velocity", {});
                    program.spawnModules.push_back(compiled);
                }
            }
            if (!ValidateParticleProgram(program, validation)) {
                result.diagnostics.push_back({emitterIndex, validation});
                continue;
            }
            program.hash = HashParticleProgram(program);
            result.programs.push_back(std::move(program));
        }
        result.succeeded = result.diagnostics.empty() && !result.programs.empty();
        if (!result.succeeded)
            result.programs.clear();
        return result;
    }
} // namespace sky::particle
