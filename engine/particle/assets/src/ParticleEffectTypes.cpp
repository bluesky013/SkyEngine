#include <particle/assets/ParticleEffectAsset.h>

#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <particle/ParticleSchema.h>

namespace sky::particle {
    void RegisterParticleAuthoringTypes(SerializationContext *context)
    {
        context->Register<ParticleAttribute>("ParticleAttribute")
            .Enum(ParticleAttribute::POSITION, "Position")
            .Enum(ParticleAttribute::VELOCITY, "Velocity")
            .Enum(ParticleAttribute::AGE, "Age")
            .Enum(ParticleAttribute::LIFETIME, "Lifetime")
            .Enum(ParticleAttribute::COLOR, "Color")
            .Enum(ParticleAttribute::SIZE, "Size")
            .Enum(ParticleAttribute::ROTATION, "Rotation")
            .Enum(ParticleAttribute::ANGULAR_VELOCITY, "AngularVelocity")
            .Enum(ParticleAttribute::SPRITE_INDEX, "SpriteIndex")
            .Enum(ParticleAttribute::SUB_UV, "SubUV")
            .Enum(ParticleAttribute::SEED, "Seed")
            .Enum(ParticleAttribute::CUSTOM0, "Custom0")
            .Enum(ParticleAttribute::CUSTOM1, "Custom1")
            .Enum(ParticleAttribute::CUSTOM2, "Custom2")
            .Enum(ParticleAttribute::CUSTOM3, "Custom3");
        context->Register<ParticleSimTarget>("ParticleSimTarget").Enum(ParticleSimTarget::CPU, "CPU");
        context->Register<ParticleSimSpace>("ParticleSimSpace")
            .Enum(ParticleSimSpace::WORLD, "World")
            .Enum(ParticleSimSpace::LOCAL, "Local");
        context->Register<ParticleRendererType>("ParticleRendererType").Enum(ParticleRendererType::SPRITE, "Sprite");
        context->Register<ParticleBlendMode>("ParticleBlendMode")
            .Enum(ParticleBlendMode::ADDITIVE, "Additive")
            .Enum(ParticleBlendMode::ALPHA, "Alpha")
            .Enum(ParticleBlendMode::PREMULTIPLIED, "Premultiplied");
        auto moduleType = context->Register<ParticleModuleType>("ParticleModuleType");
        for (const auto &schema : ParticleModuleSchemas())
            moduleType.Enum(schema.type, schema.label);
        context->Register<ParticleValueKind>("ParticleValueKind")
            .Enum(ParticleValueKind::CONSTANT, "Constant")
            .Enum(ParticleValueKind::RANDOM, "Random")
            .Enum(ParticleValueKind::CURVE, "Curve")
            .Enum(ParticleValueKind::GRADIENT, "Gradient");
        context->Register<ParticleRange>("ParticleRange").Member<&ParticleRange::min>("min").Member<&ParticleRange::max>("max");
        context->Register<ParticleCurveKey>("ParticleCurveKey").Member<&ParticleCurveKey::time>("time").Member<&ParticleCurveKey::value>("value");
        context->Register<ParticleGradientKey>("ParticleGradientKey")
            .Member<&ParticleGradientKey::time>("time")
            .Member<&ParticleGradientKey::color>("color");
        context->Register<ParticleCurve>("ParticleCurve").Member<&ParticleCurve::keys>("keys");
        context->Register<ParticleGradient>("ParticleGradient").Member<&ParticleGradient::keys>("keys");
        context->Register<ParticleValue>("ParticleValue")
            .Member<&ParticleValue::kind>("kind")
            .Member<&ParticleValue::constant>("constant")
            .Member<&ParticleValue::vector>("vector")
            .Member<&ParticleValue::range>("range")
            .Member<&ParticleValue::curve>("curve")
            .Member<&ParticleValue::gradient>("gradient")
            .Member<&ParticleValue::parameter>("parameter")
            .Member<&ParticleValue::attribute>("attribute");
        context->Register<ParticleBounds>("ParticleBounds").Member<&ParticleBounds::min>("min").Member<&ParticleBounds::max>("max");
        context->Register<ParticleSpawnDesc>("ParticleSpawnDesc")
            .Member<&ParticleSpawnDesc::rate>("rate")
            .Member<&ParticleSpawnDesc::burstInterval>("burstInterval")
            .Member<&ParticleSpawnDesc::burstCount>("burstCount")
            .Member<&ParticleSpawnDesc::warmupSeconds>("warmupSeconds");
        context->Register<ParticleRendererDesc>("ParticleRendererDesc")
            .Member<&ParticleRendererDesc::type>("type")
            .Member<&ParticleRendererDesc::blend>("blend")
            .Member<&ParticleRendererDesc::texture>("texture")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string("AuroraTexture")));
        context->Register<ParticleParameterDesc>("ParticleParameterDesc")
            .Member<&ParticleParameterDesc::name>("name")
            .Member<&ParticleParameterDesc::value>("value");
        context->Register<ParticleModuleDesc>("ParticleModuleDesc")
            .Member<&ParticleModuleDesc::type>("type")
            .Member<&ParticleModuleDesc::name>("name")
            .Member<&ParticleModuleDesc::parameters>("parameters");
        context->Register<ParticleEmitterDesc>("ParticleEmitterDesc")
            .Member<&ParticleEmitterDesc::name>("name")
            .Member<&ParticleEmitterDesc::simTarget>("simTarget")
            .Member<&ParticleEmitterDesc::space>("space")
            .Member<&ParticleEmitterDesc::capacity>("capacity")
            .Member<&ParticleEmitterDesc::spawn>("spawn")
            .Member<&ParticleEmitterDesc::bounds>("bounds")
            .Member<&ParticleEmitterDesc::modules>("modules")
            .Member<&ParticleEmitterDesc::renderers>("renderers");
        context->Register<ParticleUserParameter>("ParticleUserParameter")
            .Member<&ParticleUserParameter::name>("name")
            .Member<&ParticleUserParameter::value>("value");
        context->Register<ParticleEffectDesc>("ParticleEffectDesc")
            .Member<&ParticleEffectDesc::version>("version")
            .Member<&ParticleEffectDesc::userParameters>("userParameters")
            .Member<&ParticleEffectDesc::emitters>("emitters")
            .Member<&ParticleEffectDesc::fixedBounds>("fixedBounds");
    }
} // namespace sky::particle
