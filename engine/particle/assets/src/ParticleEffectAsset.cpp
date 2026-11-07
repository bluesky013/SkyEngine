#include <particle/assets/ParticleEffectAsset.h>

#include <framework/asset/AssetManager.h>
#include <particle/ParticleSchema.h>

#include <algorithm>

namespace sky::particle {
    namespace {
        constexpr uint32_t MAGIC        = 0x50454350;
        constexpr uint32_t VERSION      = 3;
        constexpr uint32_t MAX_PROGRAMS = MAX_PARTICLE_EMITTERS;
        constexpr uint32_t MAX_MODULES  = MAX_PARTICLE_MODULES;
        constexpr uint32_t MAX_KEYS     = MAX_PARTICLE_KEYS;

        void SaveValue(BinaryOutputArchive &archive, const Vector3 &value)
        {
            archive.SaveValue(value.x);
            archive.SaveValue(value.y);
            archive.SaveValue(value.z);
        }

        void LoadValue(BinaryInputArchive &archive, Vector3 &value)
        {
            archive.LoadValue(value.x);
            archive.LoadValue(value.y);
            archive.LoadValue(value.z);
        }

        void SaveValue(BinaryOutputArchive &archive, const Color &value)
        {
            archive.SaveValue(value.r);
            archive.SaveValue(value.g);
            archive.SaveValue(value.b);
            archive.SaveValue(value.a);
        }

        void LoadValue(BinaryInputArchive &archive, Color &value)
        {
            archive.LoadValue(value.r);
            archive.LoadValue(value.g);
            archive.LoadValue(value.b);
            archive.LoadValue(value.a);
        }

        void SaveModules(BinaryOutputArchive &archive, const std::vector<ParticleCompiledModule> &modules)
        {
            archive.SaveValue(static_cast<uint32_t>(modules.size()));
            for (const auto &module : modules) {
                archive.SaveValue(static_cast<uint32_t>(module.type));
                for (const auto value : module.values)
                    archive.SaveValue(value);
                archive.SaveValue(static_cast<uint32_t>(module.attribute));
                archive.SaveValue(static_cast<uint32_t>(module.curve.keys.size()));
                for (const auto &key : module.curve.keys) {
                    archive.SaveValue(key.time);
                    SaveValue(archive, key.value);
                }
                archive.SaveValue(static_cast<uint32_t>(module.gradient.keys.size()));
                for (const auto &key : module.gradient.keys) {
                    archive.SaveValue(key.time);
                    SaveValue(archive, key.color);
                }
                archive.SaveValue(module.randomMask);
                for (const auto value : module.randomMaximum)
                    archive.SaveValue(value);
                SaveValue(archive, module.position);
                SaveValue(archive, module.velocity);
            }
        }

        void SaveProgram(BinaryOutputArchive &archive, const ParticleProgram &program)
        {
            archive.SaveValue(program.hash);
            archive.SaveValue(program.emitter.GetStr());
            archive.SaveValue(static_cast<uint32_t>(program.flags));
            archive.SaveValue(program.layout.version);
            archive.SaveValue(program.layout.stride);
            for (const auto offset : program.layout.offsets)
                archive.SaveValue(offset);
            archive.SaveValue(program.capacity);
            SaveValue(archive, program.bounds.min);
            SaveValue(archive, program.bounds.max);
            archive.SaveValue(program.spawn.rate);
            archive.SaveValue(program.spawn.burstInterval);
            archive.SaveValue(program.spawn.burstCount);
            archive.SaveValue(program.spawn.warmupSeconds);
            archive.SaveValue(static_cast<uint32_t>(program.space));
            SaveModules(archive, program.spawnModules);
            SaveModules(archive, program.updateModules);
            archive.SaveValue(static_cast<uint32_t>(program.renderers.size()));
            for (const auto &renderer : program.renderers) {
                archive.SaveValue(static_cast<uint32_t>(renderer.type));
                archive.SaveValue(static_cast<uint32_t>(renderer.blend));
                archive.SaveValue(renderer.lit);
                archive.SaveValue(renderer.soft);
                archive.SaveValue(renderer.texture);
            }
        }

        bool LoadProgram(BinaryInputArchive &archive, ParticleProgram &program)
        {
            archive.LoadValue(program.hash);
            std::string emitter;
            archive.LoadValue(emitter);
            if (!archive.IsValid() || emitter.size() > 256)
                return false;
            program.emitter = Name(emitter.c_str());
            uint32_t flags  = 0;
            archive.LoadValue(flags);
            if (flags > 0xff)
                return false;
            program.flags = static_cast<ParticleProgramFlags>(flags);
            archive.LoadValue(program.layout.version);
            archive.LoadValue(program.layout.stride);
            for (auto &offset : program.layout.offsets)
                archive.LoadValue(offset);
            archive.LoadValue(program.capacity);
            LoadValue(archive, program.bounds.min);
            LoadValue(archive, program.bounds.max);
            archive.LoadValue(program.spawn.rate);
            archive.LoadValue(program.spawn.burstInterval);
            archive.LoadValue(program.spawn.burstCount);
            archive.LoadValue(program.spawn.warmupSeconds);
            uint32_t space = 0;
            archive.LoadValue(space);
            if (space >= static_cast<uint32_t>(ParticleSimSpace::END))
                return false;
            program.space = static_cast<ParticleSimSpace>(space);
            auto loadModules = [&](std::vector<ParticleCompiledModule> &modules) {
                uint32_t count = 0;
                archive.LoadValue(count);
                if (!archive.IsValid() || count > MAX_MODULES)
                    return false;
                modules.resize(count);
                for (auto &module : modules) {
                    uint32_t type = 0;
                    archive.LoadValue(type);
                    if (type >= static_cast<uint32_t>(ParticleModuleType::END))
                        return false;
                    module.type = static_cast<ParticleModuleType>(type);
                    for (auto &value : module.values)
                        archive.LoadValue(value);
                    uint32_t attribute = 0;
                    archive.LoadValue(attribute);
                    if (attribute >= PARTICLE_ATTRIBUTE_COUNT)
                        return false;
                    module.attribute = static_cast<ParticleAttribute>(attribute);
                    uint32_t keys    = 0;
                    archive.LoadValue(keys);
                    if (!archive.IsValid() || keys > MAX_KEYS)
                        return false;
                    module.curve.keys.resize(keys);
                    for (auto &key : module.curve.keys) {
                        archive.LoadValue(key.time);
                        LoadValue(archive, key.value);
                    }
                    archive.LoadValue(keys);
                    if (!archive.IsValid() || keys > MAX_KEYS)
                        return false;
                    module.gradient.keys.resize(keys);
                    for (auto &key : module.gradient.keys) {
                        archive.LoadValue(key.time);
                        LoadValue(archive, key.color);
                    }
                    archive.LoadValue(module.randomMask);
                    for (auto &value : module.randomMaximum)
                        archive.LoadValue(value);
                    LoadValue(archive, module.position);
                    LoadValue(archive, module.velocity);
                }
                return archive.IsValid();
            };
            if (!loadModules(program.spawnModules) || !loadModules(program.updateModules))
                return false;
            uint32_t renderers = 0;
            archive.LoadValue(renderers);
            if (!archive.IsValid() || renderers > MAX_MODULES)
                return false;
            program.renderers.resize(renderers);
            for (auto &renderer : program.renderers) {
                uint32_t type = 0, blend = 0;
                archive.LoadValue(type);
                archive.LoadValue(blend);
                archive.LoadValue(renderer.lit);
                archive.LoadValue(renderer.soft);
                archive.LoadValue(renderer.texture);
                if (type >= static_cast<uint32_t>(ParticleRendererType::END) || blend >= static_cast<uint32_t>(ParticleBlendMode::END))
                    return false;
                renderer.type  = static_cast<ParticleRendererType>(type);
                renderer.blend = static_cast<ParticleBlendMode>(blend);
            }
            std::string error;
            if (!archive.IsValid() || !ValidateParticleProgram(program, error))
                return false;
            const auto hash = HashParticleProgram(program);
            if (hash != program.hash)
                return false;
            program.hash = hash;
            return true;
        }
    } // namespace

    void ParticleEffectAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(MAGIC);
        archive.SaveValue(VERSION);
        archive.SaveValue(version);
        archive.SaveValue(sourceHash);
        archive.SaveValue(static_cast<uint32_t>(programs.size()));
        for (const auto &program : programs)
            SaveProgram(archive, program);
    }

    void ParticleEffectAssetData::Load(BinaryInputArchive &archive)
    {
        programs.clear();
        valid          = false;
        uint32_t magic = 0, fileVersion = 0, count = 0;
        archive.LoadValue(magic);
        archive.LoadValue(fileVersion);
        archive.LoadValue(version);
        archive.LoadValue(sourceHash);
        archive.LoadValue(count);
        if (!archive.IsValid() || magic != MAGIC || fileVersion != VERSION || count > MAX_PROGRAMS)
            return;
        programs.resize(count);
        for (auto &program : programs) {
            if (!LoadProgram(archive, program)) {
                programs.clear();
                return;
            }
        }
        valid = archive.IsValid();
    }

    void RegisterParticleAssets(SerializationContext *context)
    {
        context->Register<ParticleEffectAssetData>("ParticleEffectAssetData")
            .BinLoad<&ParticleEffectAssetData::Load>()
            .BinSave<&ParticleEffectAssetData::Save>();
        AssetManager::Get()->RegisterAssetHandler<ParticleEffectAsset>();
    }
} // namespace sky::particle
