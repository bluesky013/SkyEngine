//
// Created by blues on 2024/9/2.
//

#include <physics/PhysicsRegistry.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::phy {

    void PhysicsRegistry::Reflect(SerializationContext *context)
    {
        context->Register<SphereShape>("PhysicsSphere")
            .Member<&SphereShape::pivot>("pivot")
            .Member<&SphereShape::radius>("radis");

        context->Register<BoxShape>("PhysicsBox")
            .Member<&BoxShape::pivot>("pivot")
            .Member<&BoxShape::halfExt>("halfExt");

        context->Register<TriangleMeshShape>("TriangleMeshShape")
            .Member<&TriangleMeshShape::asset>("asset");

        context->Register<MeshPhysicsConfig>("MeshPhysicsConfig")
            .Member<&MeshPhysicsConfig::sphere>("sphere")
            .Member<&MeshPhysicsConfig::box>("boxes")
            .Member<&MeshPhysicsConfig::tris>("tris");
    }

    void PhysicsRegistry::GatherConfigTypes(std::set<Uuid> &typeId)
    {
        typeId.emplace(TypeInfo<MeshPhysicsConfig>::RegisteredId());
    }

    PhysicsWorld* PhysicsRegistry::CreatePhysicsWorld()
    {
        return factory ? factory->CreatePhysicsWorld() : nullptr;
    }

    CharacterController* PhysicsRegistry::CreateCharacterController()
    {
        return factory ? factory->CreateCharacterController() : nullptr;
    }

    CollisionObject* PhysicsRegistry::CreateCollisionObject()
    {
        return factory ? factory->CreateCollisionObject() : nullptr;
    }

    RigidBody* PhysicsRegistry::CreateRigidBody()
    {
        return factory ? factory->CreateRigidBody() : nullptr;
    }

    IShapeImpl* PhysicsRegistry::CreateBox(const BoxShape& shape)
    {
        return factory ? factory->CreateBox(shape) : nullptr;
    }

    IShapeImpl* PhysicsRegistry::CreateSphere(const SphereShape& shape)
    {
        return factory ? factory->CreateSphere(shape) : nullptr;
    }

    IShapeImpl* PhysicsRegistry::CreateTriangleMesh(const TriangleMeshShape& shape)
    {
        return factory ? factory->CreateTriangleMesh(shape) : nullptr;
    }

    void PhysicsRegistry::Register(Impl* impl)
    {
        factory.reset(impl);
    }

    void PhysicsRegistry::UnRegister()
    {
        factory.reset();
    }
} // namespace sky::phy