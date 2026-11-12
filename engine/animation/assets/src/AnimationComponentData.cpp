#include <animation/assets/AnimationComponentData.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/WorldSubSystemRegistry.h>

namespace sky::animation {
    void RegisterAnimationComponentData(SerializationContext *context)
    {
        context->Register<AnimationMode>("AnimationMode")
            .Enum(AnimationMode::Clip, "Clip")
            .Enum(AnimationMode::Graph, "Graph")
            .Enum(AnimationMode::ReferencePose, "ReferencePose");
        context->Register<AnimationRootPolicy>("AnimationRootPolicy")
            .Enum(AnimationRootPolicy::KeepInPose, "KeepInPose")
            .Enum(AnimationRootPolicy::LockToReference, "LockToReference");
        context->Register<AnimationParameterOverride>("AnimationParameterOverride")
            .Member<&AnimationParameterOverride::parameter>("parameter")
            .Member<&AnimationParameterOverride::isBoolean>("isBoolean")
            .Member<&AnimationParameterOverride::booleanValue>("booleanValue")
            .Member<&AnimationParameterOverride::floatValue>("floatValue");
        context->Register<AnimationComponentData>("AnimationComponentData")
            .Member<&AnimationComponentData::parameters>("parameters")
            .Member<&AnimationComponentData::mode>("mode")
            .Member<&AnimationComponentData::skeleton>("skeleton")
            .Member<&AnimationComponentData::clip>("clip")
            .Member<&AnimationComponentData::graph>("graph")
            .Member<&AnimationComponentData::autoplay>("autoplay")
            .Member<&AnimationComponentData::loop>("loop")
            .Member<&AnimationComponentData::playRate>("playRate")
            .Member<&AnimationComponentData::rootPolicy>("rootPolicy");
        const bool registered = ComponentFactory::Get()->RegisterPersistentDataType(Uuid::CreateWithSeed(Fnv1a32("AnimationComponent")),
                                                                                    TypeInfoObj<AnimationComponentData>::Get()->RtInfo());
        SKY_ASSERT(registered);
        const bool subsystem = WorldSubSystemRegistry::Get().RegisterPersistentSchema(Name("Animation"), {});
        SKY_ASSERT(subsystem);
    }
} // namespace sky::animation
