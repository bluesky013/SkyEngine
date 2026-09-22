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

        context->Register<CapsuleShape>("PhysicsCapsule")
            .Member<&CapsuleShape::pivot>("pivot")
            .Member<&CapsuleShape::radius>("radius")
            .Member<&CapsuleShape::height>("height");

        context->Register<HeightFieldShape>("PhysicsHeightField")
            .Member<&HeightFieldShape::width>("width")
            .Member<&HeightFieldShape::height>("height")
            .Member<&HeightFieldShape::scaleX>("scaleX")
            .Member<&HeightFieldShape::scaleZ>("scaleZ")
            .Member<&HeightFieldShape::heightScale>("heightScale")
            .Member<&HeightFieldShape::heightOffset>("heightOffset")
            .Member<&HeightFieldShape::minHeight>("minHeight")
            .Member<&HeightFieldShape::maxHeight>("maxHeight");

        context->Register<MeshPhysicsConfig>("MeshPhysicsConfig")
            .Member<&MeshPhysicsConfig::sphere>("sphere")
            .Member<&MeshPhysicsConfig::box>("boxes")
            .Member<&MeshPhysicsConfig::mesh>("mesh");
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

    IShapeImpl* PhysicsRegistry::CreateHeightField(const HeightFieldShape& shape)
    {
        return factory ? factory->CreateHeightField(shape) : nullptr;
    }

    IShapeImpl* PhysicsRegistry::CreateCapsule(const CapsuleShape& shape)
    {
        return factory ? factory->CreateCapsule(shape) : nullptr;
    }

    IMaterialImpl* PhysicsRegistry::CreateMaterial(const PhysicsMaterialData& data)
    {
        return factory ? factory->CreateMaterial(data) : nullptr;
    }

    void PhysicsRegistry::Register(Impl* impl)
    {
        factory.reset(impl);
        if (factory) {
            factory->Init();
        }
    }

    void PhysicsRegistry::UnRegister()
    {
        if (factory) {
            factory->Shutdown();
        }
        factory.reset();
    }
} // namespace sky::phy