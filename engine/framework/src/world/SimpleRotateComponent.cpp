//
// Created by blues on 2025/2/21.
//

#include <framework/world/SimpleRotateComponent.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/Actor.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    void SimpleRotateComponent::Reflect(SerializationContext *context)
    {
        context->Register<SimpleRotateData>("SimpleRotateData")
            .Member<&SimpleRotateData::speed>("Speed");

        REGISTER_BEGIN(SimpleRotateComponent, context)
        REGISTER_MEMBER(Speed, SetSpeed, GetSpeed);

        ComponentFactory::Get()->RegisterComponent<SimpleRotateComponent>("Base");
    }

    void SimpleRotateComponent::Tick(float time)
    {
        angle += time * data.speed;

        auto *trans = actor->GetComponent<TransformComponent>();

        Quaternion quad;
        quad.FromEulerYZX(Vector3{0, angle, 0.f});
        trans->SetLocalRotation(quad);
    }

} // namespace sky