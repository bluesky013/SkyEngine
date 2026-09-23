//
// Created on 2026/09/23.
//

#include <bullet/BulletBackend.h>
#include <bullet/BulletConversion.h>

#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace sky::phy {

    namespace {

        constexpr int kMaxFilterBits = 0xFFFF;

        btTransform MakeFrame(const Vector3 &pivot, const Vector3 &axis)
        {
            btTransform frame = btTransform::getIdentity();
            frame.setOrigin(ToBullet(pivot));
            const float axisLength = axis.Length();
            if (axisLength > 0.f) {
                const btVector3 from(0.f, 0.f, 1.f);
                const btVector3 to = ToBullet(axis / axisLength);
                const btScalar dot = from.dot(to);
                if (dot > 0.9999f) {
                    return frame;
                }
                if (dot < -0.9999f) {
                    frame.setRotation(btQuaternion(btVector3(1, 0, 0), 3.14159265358979323846f));
                    return frame;
                }
                const btVector3 axisV = from.cross(to);
                frame.setRotation(btQuaternion(axisV, std::acos(dot)));
            }
            return frame;
        }

        Transform LerpTransform(const Transform &a, const Transform &b, float t)
        {
            Transform result;
            result.translation = a.translation + (b.translation - a.translation) * t;
            result.scale       = a.scale + (b.scale - a.scale) * t;

            Quaternion qb = b.rotation;
            if (a.rotation.Dot(qb) < 0.f) {
                for (int i = 0; i < 4; ++i) {
                    qb.v[i] = -qb.v[i];
                }
            }
            Quaternion q;
            for (int i = 0; i < 4; ++i) {
                q.v[i] = a.rotation.v[i] * (1.f - t) + qb.v[i] * t;
            }
            q.Normalize();
            result.rotation = q;
            return result;
        }

        // Debug drawer that captures Bullet's debug output into render-agnostic geometry.
        class CaptureDebugDraw : public btIDebugDraw {
        public:
            explicit CaptureDebugDraw(PhysicsDebugGeometry &out) : geometry(out) {}

            void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color) override
            {
                const float c[4] = {color.x(), color.y(), color.z(), 1.f};
                geometry.AddLine(FromBullet(from), FromBullet(to), c);
            }
            void drawContactPoint(const btVector3 &, const btVector3 &, btScalar, int, const btVector3 &) override {}
            void reportErrorWarning(const char *) override {}
            void draw3dText(const btVector3 &, const char *) override {}
            void setDebugMode(int mode) override { debugMode = mode; }
            int  getDebugMode() const override { return debugMode; }

        private:
            PhysicsDebugGeometry &geometry;
            int                   debugMode = 0;
        };

    } // namespace

    BulletBackend::BulletBackend()
    {
        caps.mathMode      = PhysicsMathMode::Fast;
        caps.deterministic = false;
        caps.jobStepping   = false;
        caps.supportsCCD   = true;
        caps.supportsConstraints = true;
        caps.supportsCharacters  = true;
        caps.supportsBox          = true;
        caps.supportsSphere       = true;
        caps.supportsCapsule      = true;
        caps.supportsHeightField  = true;
        caps.supportsTriangleMesh = true;
        caps.supportsConvexHull   = true;
        caps.supportsCompound     = true;
    }

    IPhysicsWorld *BulletBackend::CreateWorld(const PhysicsWorldDesc &desc)
    {
        return new BulletBackendWorld(desc);
    }

    void BulletBackend::DestroyWorld(IPhysicsWorld *world)
    {
        delete world;
    }

    BulletBackendWorld::BulletBackendWorld(const PhysicsWorldDesc &desc)
    {
        caps                      = PhysicsBackendCaps{};
        caps.mathMode             = PhysicsMathMode::Fast;
        caps.deterministic        = false;
        caps.supportsCCD          = true;
        caps.supportsConstraints  = true;
        caps.supportsCharacters   = true;
        caps.supportsHeightField  = true;
        caps.supportsTriangleMesh = true;
        caps.supportsConvexHull   = true;
        caps.supportsCompound     = true;

        configuration = std::make_unique<btDefaultCollisionConfiguration>();
        dispatcher    = std::make_unique<btCollisionDispatcher>(configuration.get());
        broadPhase    = std::make_unique<btDbvtBroadphase>();
        solver        = std::make_unique<btSequentialImpulseConstraintSolver>();

        broadPhase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());

        dynamicWorld = std::make_unique<btDiscreteDynamicsWorld>(
            dispatcher.get(), broadPhase.get(), solver.get(), configuration.get());
        dynamicWorld->setGravity(ToBullet(desc.gravity));
        options = desc.options;
    }

    BulletBackendWorld::~BulletBackendWorld()
    {
        for (auto &entry : slots) {
            if (!entry) {
                continue;
            }
            switch (entry->kind) {
                case ObjectKind::Constraint:
                    if (entry->constraint) {
                        dynamicWorld->removeConstraint(entry->constraint.get());
                    }
                    break;
                case ObjectKind::Character:
                    if (entry->controller) {
                        dynamicWorld->removeAction(entry->controller.get());
                    }
                    if (entry->ghost) {
                        dynamicWorld->removeCollisionObject(entry->ghost.get());
                    }
                    break;
                case ObjectKind::Body:
                    if (entry->body) {
                        dynamicWorld->removeRigidBody(entry->body.get());
                    }
                    break;
            }
        }
        slots.clear();
    }

    BulletBackendWorld::ObjectEntry *BulletBackendWorld::Resolve(PhysicsObjectId id) const
    {
        if (id.index == 0 || id.index > slots.size()) {
            return nullptr;
        }
        const uint32_t slot = static_cast<uint32_t>(id.index - 1);
        if (generations[slot] != id.generation || !slots[slot]) {
            return nullptr;
        }
        return slots[slot].get();
    }

    PhysicsObjectId BulletBackendWorld::AllocateId(ObjectKind kind)
    {
        uint32_t slot = 0;
        if (!freeSlots.empty()) {
            slot = freeSlots.back();
            freeSlots.pop_back();
            generations[slot]++;
        } else {
            slots.emplace_back(nullptr);
            generations.push_back(1);
            slot = static_cast<uint32_t>(slots.size() - 1);
        }

        auto entry = std::make_unique<ObjectEntry>();
        entry->id  = PhysicsObjectId{static_cast<uint64_t>(slot) + 1, generations[slot]};
        entry->kind = kind;
        entry->tag  = std::make_unique<BulletObjectTag>();
        entry->tag->id = entry->id;

        slots[slot] = std::move(entry);
        return slots[slot]->id;
    }

    void BulletBackendWorld::Release(uint32_t slot)
    {
        if (slot < slots.size()) {
            slots[slot].reset();
            freeSlots.push_back(slot);
        }
    }

    PhysicsMaterialId BulletBackendWorld::CreateMaterial(const PhysicsMaterialData &data)
    {
        auto entry = std::make_unique<MaterialEntry>();
        entry->data = data;
        materials.push_back(std::move(entry));
        return PhysicsMaterialId{static_cast<uint64_t>(materials.size()), 1};
    }

    bool BulletBackendWorld::DestroyMaterial(PhysicsMaterialId id)
    {
        if (id.index == 0 || id.index > materials.size()) {
            return false;
        }
        materials[id.index - 1].reset();
        return true;
    }

    bool BulletBackendWorld::GetMaterial(PhysicsMaterialId id, PhysicsMaterialData &out) const
    {
        if (id.index == 0 || id.index > materials.size() || !materials[id.index - 1]) {
            return false;
        }
        out = materials[id.index - 1]->data;
        return true;
    }

    std::unique_ptr<btCollisionShape> BulletBackendWorld::CookShape(const ShapeDesc &desc, ObjectEntry &entry) const
    {
        switch (desc.type) {
            case ShapeType::Box:
                return std::make_unique<btBoxShape>(ToBullet(desc.halfExt));
            case ShapeType::Sphere:
                return std::make_unique<btSphereShape>(desc.radius);
            case ShapeType::Capsule:
                return std::make_unique<btCapsuleShape>(desc.radius, desc.height);
            case ShapeType::HeightField: {
                entry.heightSamples = desc.samples;
                auto shape = std::make_unique<btHeightfieldTerrainShape>(
                    static_cast<int>(desc.cols), static_cast<int>(desc.rows), entry.heightSamples.data(),
                    desc.heightScale, desc.minHeight, desc.maxHeight, desc.upAxis, PHY_FLOAT, false);
                shape->setLocalScaling(btVector3(desc.scaleX, 1.f, desc.scaleZ));
                return shape;
            }
            case ShapeType::TriangleMesh: {
                if (!desc.meshData) {
                    return nullptr;
                }
                auto mesh = std::make_unique<btTriangleMesh>();
                const TriangleMesh &data = *desc.meshData;
                const auto indexAt = [&data](uint32_t i) -> uint32_t {
                    if (data.indexType == IndexType::U16) {
                        return reinterpret_cast<const uint16_t *>(data.indexRaw.data())[i];
                    }
                    return reinterpret_cast<const uint32_t *>(data.indexRaw.data())[i];
                };
                const auto vertexAt = [&data](uint32_t i) -> Vector3 {
                    return *reinterpret_cast<const Vector3 *>(data.position.data() + i * data.vtxStride);
                };
                const uint32_t triCount = static_cast<uint32_t>(data.indexRaw.size() /
                    (data.indexType == IndexType::U16 ? sizeof(uint16_t) : sizeof(uint32_t))) / 3u;
                for (uint32_t t = 0; t < triCount; ++t) {
                    mesh->addTriangle(ToBullet(vertexAt(indexAt(t * 3 + 0))),
                                      ToBullet(vertexAt(indexAt(t * 3 + 1))),
                                      ToBullet(vertexAt(indexAt(t * 3 + 2))));
                }
                auto shape = std::make_unique<btBvhTriangleMeshShape>(mesh.get(), true);
                entry.meshInterface = std::move(mesh);
                return shape;
            }
            case ShapeType::ConvexHull: {
                auto hull = std::make_unique<btConvexHullShape>();
                for (const auto &p : desc.points) {
                    hull->addPoint(ToBullet(p), false);
                }
                hull->recalcLocalAabb();
                return hull;
            }
            case ShapeType::Compound: {
                auto compound = std::make_unique<btCompoundShape>();
                for (const auto &child : desc.children) {
                    auto childShape = CookShape(child, entry);
                    if (childShape) {
                        const btTransform xform(btQuaternion::getIdentity(), ToBullet(child.pivot));
                        compound->addChildShape(xform, childShape.release());
                    }
                }
                return compound;
            }
        }
        return nullptr;
    }

    void BulletBackendWorld::ApplyFilter(btCollisionObject &object, const CollisionFilter &filter) const
    {
        (void)object;
        (void)filter;
    }

    void BulletBackendWorld::ApplyMaterial(btRigidBody &body, const PhysicsMaterialData &material) const
    {
        body.setFriction(material.dynamicFriction);
        body.setRestitution(material.restitution);
        body.setDamping(material.linearDamping, material.angularDamping);
    }

    PhysicsObjectId BulletBackendWorld::CreateBody(const PhysicsBodyDesc &desc)
    {
        std::string why;
        if (!ValidateBodyDesc(desc, &why)) {
            return INVALID_PHYSICS_OBJECT_ID;
        }
        if (desc.shape.type == ShapeType::TriangleMesh && desc.kind == BodyKind::Dynamic &&
            !options.allowDynamicTriangleMesh) {
            return INVALID_PHYSICS_OBJECT_ID;
        }
        if (options.maxBodies > 0u) {
            uint32_t bodyCount = 0;
            for (const auto &slot : slots) {
                if (slot && slot->kind == ObjectKind::Body) {
                    ++bodyCount;
                }
            }
            if (bodyCount >= options.maxBodies) {
                return INVALID_PHYSICS_OBJECT_ID;
            }
        }

        const PhysicsObjectId id    = AllocateId(ObjectKind::Body);
        ObjectEntry         *entry  = Resolve(id);
        entry->filter      = desc.filter;
        entry->isTrigger   = desc.isTrigger;
        entry->tag->filter = desc.filter;
        entry->tag->isTrigger = desc.isTrigger;
        entry->curr        = desc.transform;
        entry->prev        = desc.transform;

        entry->shape = CookShape(desc.shape, *entry);
        if (!entry->shape) {
            Release(static_cast<uint32_t>(id.index - 1));
            return INVALID_PHYSICS_OBJECT_ID;
        }

        btVector3 inertia(0, 0, 0);
        btScalar  mass = 0.f;
        if (desc.kind == BodyKind::Dynamic) {
            mass = desc.mass;
            if (desc.inertia.x <= 0.f && desc.inertia.y <= 0.f && desc.inertia.z <= 0.f) {
                entry->shape->calculateLocalInertia(mass, inertia);
            } else {
                inertia = ToBullet(desc.inertia);
            }
        }

        entry->motion = std::make_unique<btDefaultMotionState>(ToBullet(desc.transform));
        entry->body   = std::make_unique<btRigidBody>(
            btRigidBody::btRigidBodyConstructionInfo(mass, entry->motion.get(), entry->shape.get(), inertia));
        entry->body->setUserPointer(entry->tag.get());

        if (desc.kind == BodyKind::Static) {
            entry->body->setCollisionFlags(entry->body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
        } else if (desc.kind == BodyKind::Kinematic) {
            entry->body->setCollisionFlags(entry->body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            entry->body->setActivationState(DISABLE_DEACTIVATION);
        }
        if (desc.isTrigger) {
            entry->body->setCollisionFlags(entry->body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
        }
        if (!desc.gravityEnabled) {
            entry->body->setGravity(btVector3(0, 0, 0));
        }
        if (!desc.allowSleep || !options.enableSleep) {
            entry->body->setActivationState(DISABLE_DEACTIVATION);
        } else if (desc.startsAsleep) {
            entry->body->setActivationState(ISLAND_SLEEPING);
        }
        if (desc.enableCCD && caps.supportsCCD && options.enableCCD) {
            entry->body->setCcdMotionThreshold(1e-3f);
            const btScalar r = std::min<btScalar>(0.2f, entry->shape->getLocalScaling().getX() * 0.5f + 0.05f);
            entry->body->setCcdSweptSphereRadius(r);
        }

        entry->material = GetDefaultPhysicsMaterial();
        entry->hasExplicitMaterial = false;
        if (IsValid(desc.material)) {
            PhysicsMaterialData data;
            if (GetMaterial(desc.material, data)) {
                entry->material = data;
                entry->hasExplicitMaterial = true;
            }
        }
        ApplyMaterial(*entry->body, entry->material);

        dynamicWorld->addRigidBody(entry->body.get(),
            static_cast<int>(desc.filter.group & kMaxFilterBits),
            static_cast<int>(desc.filter.mask & kMaxFilterBits));
        return id;
    }

    bool BulletBackendWorld::DestroyObject(PhysicsObjectId id)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry) {
            return false;
        }
        const uint32_t slot = static_cast<uint32_t>(id.index - 1);

        switch (entry->kind) {
            case ObjectKind::Constraint:
                if (entry->constraint) {
                    dynamicWorld->removeConstraint(entry->constraint.get());
                }
                break;
            case ObjectKind::Character:
                if (entry->controller) {
                    dynamicWorld->removeAction(entry->controller.get());
                }
                if (entry->ghost) {
                    dynamicWorld->removeCollisionObject(entry->ghost.get());
                }
                break;
            case ObjectKind::Body:
                if (entry->body) {
                    dynamicWorld->removeRigidBody(entry->body.get());
                }
                break;
        }

        Release(slot);
        return true;
    }

    bool BulletBackendWorld::HasObject(PhysicsObjectId id) const
    {
        return Resolve(id) != nullptr;
    }

    bool BulletBackendWorld::GetBodyTransform(PhysicsObjectId id, Transform &out) const
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry) {
            return false;
        }
        if (entry->kind == ObjectKind::Character && entry->ghost) {
            out = FromBullet(entry->ghost->getWorldTransform());
            return true;
        }
        if (!entry->body) {
            return false;
        }
        out = FromBullet(entry->body->getWorldTransform());
        return true;
    }

    bool BulletBackendWorld::SetBodyTransform(PhysicsObjectId id, const Transform &transform)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry) {
            return false;
        }
        if (entry->kind == ObjectKind::Character && entry->ghost) {
            entry->ghost->setWorldTransform(ToBullet(transform));
            return true;
        }
        if (!entry->body) {
            return false;
        }
        entry->body->setWorldTransform(ToBullet(transform));
        if (entry->motion) {
            entry->motion->setWorldTransform(ToBullet(transform));
        }
        entry->body->activate(true);
        return true;
    }

    void BulletBackendWorld::SetInterpolationAlpha(float alpha)
    {
        interpolationAlpha = alpha < 0.f ? 0.f : (alpha > 1.f ? 1.f : alpha);
    }

    bool BulletBackendWorld::GetInterpolatedTransform(PhysicsObjectId id, Transform &out) const
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry) {
            return false;
        }

        Transform current;
        if (entry->kind == ObjectKind::Character && entry->ghost) {
            current = FromBullet(entry->ghost->getWorldTransform());
        } else if (entry->body) {
            current = FromBullet(entry->body->getWorldTransform());
        } else {
            return false;
        }

        if (interpolationAlpha <= 0.f) {
            out = current;
            return true;
        }
        out = LerpTransform(entry->prev, current, interpolationAlpha);
        return true;
    }

    bool BulletBackendWorld::GetBodyVelocity(PhysicsObjectId id, Vector3 &linear, Vector3 &angular) const
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Body || !entry->body) {
            return false;
        }
        linear  = FromBullet(entry->body->getLinearVelocity());
        angular = FromBullet(entry->body->getAngularVelocity());
        return true;
    }

    bool BulletBackendWorld::SetBodyVelocity(PhysicsObjectId id, const Vector3 &linear, const Vector3 &angular)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Body || !entry->body) {
            return false;
        }
        entry->body->setLinearVelocity(ToBullet(linear));
        entry->body->setAngularVelocity(ToBullet(angular));
        entry->body->activate(true);
        return true;
    }

    bool BulletBackendWorld::ApplyForce(PhysicsObjectId id, const Vector3 &force, const Vector3 &atWorldPos)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Body || !entry->body) {
            return false;
        }
        entry->body->applyForce(ToBullet(force), ToBullet(atWorldPos));
        entry->body->activate(true);
        return true;
    }

    bool BulletBackendWorld::ApplyImpulse(PhysicsObjectId id, const Vector3 &impulse, const Vector3 &atWorldPos)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Body || !entry->body) {
            return false;
        }
        entry->body->applyImpulse(ToBullet(impulse), ToBullet(atWorldPos));
        entry->body->activate(true);
        return true;
    }

    bool BulletBackendWorld::SetBodyFilter(PhysicsObjectId id, const CollisionFilter &filter)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || !entry->tag) {
            return false;
        }
        entry->filter      = filter;
        entry->tag->filter = filter;
        return true;
    }

    bool BulletBackendWorld::SetBodyMaterial(PhysicsObjectId id, PhysicsMaterialId material)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || !entry->body) {
            return false;
        }
        PhysicsMaterialData data;
        if (!GetMaterial(material, data)) {
            return false;
        }
        entry->material            = data;
        entry->hasExplicitMaterial = true;
        ApplyMaterial(*entry->body, data);
        return true;
    }

    bool BulletBackendWorld::Wake(PhysicsObjectId id)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Body || !entry->body) {
            return false;
        }
        entry->body->activate(true);
        return true;
    }

    PhysicsObjectId BulletBackendWorld::CreateConstraint(const ConstraintDesc &desc)
    {
        std::string why;
        if (!ValidateConstraintDesc(desc, &why)) {
            return INVALID_PHYSICS_OBJECT_ID;
        }
        ObjectEntry *entryA = Resolve(desc.bodyA);
        ObjectEntry *entryB = Resolve(desc.bodyB);
        btRigidBody &a = (entryA && entryA->body) ? *entryA->body : btTypedConstraint::getFixedBody();
        btRigidBody &b = (entryB && entryB->body) ? *entryB->body : btTypedConstraint::getFixedBody();

        const btTransform frameA = MakeFrame(desc.pivotA, desc.axisA);
        const btTransform frameB = MakeFrame(desc.pivotB, desc.axisB);

        std::unique_ptr<btTypedConstraint> constraint;
        switch (desc.type) {
            case ConstraintType::Fixed:
                constraint = std::make_unique<btFixedConstraint>(a, b, frameA, frameB);
                break;
            case ConstraintType::Hinge:
                constraint = std::make_unique<btHingeConstraint>(
                    a, b, ToBullet(desc.pivotA), ToBullet(desc.pivotB), ToBullet(desc.axisA), ToBullet(desc.axisB));
                break;
            case ConstraintType::Slider:
                constraint = std::make_unique<btSliderConstraint>(a, b, frameA, frameB, true);
                break;
            case ConstraintType::Distance:
                constraint = std::make_unique<btPoint2PointConstraint>(
                    a, b, ToBullet(desc.pivotA), ToBullet(desc.pivotB));
                break;
            case ConstraintType::Generic6DOF:
                constraint = std::make_unique<btGeneric6DofConstraint>(a, b, frameA, frameB, true);
                break;
        }
        if (!constraint) {
            return INVALID_PHYSICS_OBJECT_ID;
        }

        const PhysicsObjectId id = AllocateId(ObjectKind::Constraint);
        ObjectEntry *entry = Resolve(id);
        entry->constraint  = std::move(constraint);
        entry->constraintA = desc.bodyA;
        entry->constraintB = desc.bodyB;

        dynamicWorld->addConstraint(entry->constraint.get(), !desc.collideConnected);
        return id;
    }

    bool BulletBackendWorld::DestroyConstraint(PhysicsObjectId id)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Constraint) {
            return false;
        }
        if (entry->constraint) {
            dynamicWorld->removeConstraint(entry->constraint.get());
        }
        Release(static_cast<uint32_t>(id.index - 1));
        return true;
    }

    PhysicsObjectId BulletBackendWorld::CreateCharacter(const CharacterDesc &desc)
    {
        std::string why;
        if (!ValidateCharacterDesc(desc, &why)) {
            return INVALID_PHYSICS_OBJECT_ID;
        }

        const PhysicsObjectId id = AllocateId(ObjectKind::Character);
        ObjectEntry *entry = Resolve(id);
        entry->filter      = desc.filter;
        entry->tag->filter = desc.filter;
        entry->curr        = desc.transform;
        entry->prev        = desc.transform;

        entry->characterShape = std::make_unique<btCapsuleShape>(desc.radius, desc.height);
        entry->ghost          = std::make_unique<btPairCachingGhostObject>();
        entry->ghost->setCollisionShape(entry->characterShape.get());
        entry->ghost->setWorldTransform(ToBullet(desc.transform));
        entry->ghost->setUserPointer(entry->tag.get());
        entry->ghost->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

        const btVector3 up = desc.up.x > 0.5f ? btVector3(1, 0, 0)
                            : (desc.up.z > 0.5f ? btVector3(0, 0, 1) : btVector3(0, 1, 0));
        entry->controller = std::make_unique<btKinematicCharacterController>(
            entry->ghost.get(), entry->characterShape.get(), desc.stepHeight, up);
        entry->controller->setGravity(btVector3(0.f, -desc.gravity, 0.f));
        entry->controller->setMaxSlope(btRadians(desc.slopeLimit));

        dynamicWorld->addCollisionObject(entry->ghost.get(),
            static_cast<int>(desc.filter.group & kMaxFilterBits), static_cast<int>(desc.filter.mask & kMaxFilterBits));
        dynamicWorld->addAction(entry->controller.get());
        return id;
    }

    bool BulletBackendWorld::DestroyCharacter(PhysicsObjectId id)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Character) {
            return false;
        }
        if (entry->controller) {
            dynamicWorld->removeAction(entry->controller.get());
        }
        if (entry->ghost) {
            dynamicWorld->removeCollisionObject(entry->ghost.get());
        }
        Release(static_cast<uint32_t>(id.index - 1));
        return true;
    }

    CharacterMoveResult BulletBackendWorld::MoveCharacter(PhysicsObjectId id, const Vector3 &displacement)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Character || !entry->controller) {
            return CharacterMoveResult::NotAttached;
        }
        entry->controller->setWalkDirection(ToBullet(displacement));
        return CharacterMoveResult::Applied;
    }

    bool BulletBackendWorld::GetCharacterState(PhysicsObjectId id, CharacterState &out) const
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Character || !entry->ghost) {
            return false;
        }
        out.transform = FromBullet(entry->ghost->getWorldTransform());
        out.grounded  = entry->controller ? entry->controller->onGround() : false;
        return true;
    }

    bool BulletBackendWorld::SetCharacterTransform(PhysicsObjectId id, const Transform &transform)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Character || !entry->ghost) {
            return false;
        }
        entry->ghost->setWorldTransform(ToBullet(transform));
        return true;
    }

    bool BulletBackendWorld::SetCharacterCapsule(PhysicsObjectId id, float radius, float height)
    {
        ObjectEntry *entry = Resolve(id);
        if (!entry || entry->kind != ObjectKind::Character || !entry->ghost) {
            return false;
        }
        const Transform current = FromBullet(entry->ghost->getWorldTransform());
        dynamicWorld->removeAction(entry->controller.get());
        dynamicWorld->removeCollisionObject(entry->ghost.get());

        entry->characterShape = std::make_unique<btCapsuleShape>(radius, height);
        entry->ghost = std::make_unique<btPairCachingGhostObject>();
        entry->ghost->setCollisionShape(entry->characterShape.get());
        entry->ghost->setWorldTransform(ToBullet(current));
        entry->ghost->setUserPointer(entry->tag.get());
        entry->ghost->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

        entry->controller = std::make_unique<btKinematicCharacterController>(
            entry->ghost.get(), entry->characterShape.get(), 0.4f, btVector3(0, 1, 0));
        dynamicWorld->addCollisionObject(entry->ghost.get(),
            static_cast<int>(entry->filter.group & kMaxFilterBits), static_cast<int>(entry->filter.mask & kMaxFilterBits));
        dynamicWorld->addAction(entry->controller.get());
        return true;
    }

    void BulletBackendWorld::SyncTransforms()
    {
        for (auto &slot : slots) {
            if (!slot) {
                continue;
            }
            if (slot->kind == ObjectKind::Body && slot->body) {
                slot->prev = slot->curr;
                slot->curr = FromBullet(slot->body->getWorldTransform());
            } else if (slot->kind == ObjectKind::Character && slot->ghost) {
                slot->prev = slot->curr;
                slot->curr = FromBullet(slot->ghost->getWorldTransform());
            }
        }
    }

    void BulletBackendWorld::GatherContactPairs(std::vector<std::pair<PhysicsObjectId, PhysicsObjectId>> &pairs) const
    {
        const int manifoldCount = dispatcher->getNumManifolds();
        pairs.clear();
        for (int i = 0; i < manifoldCount; ++i) {
            const btPersistentManifold *manifold = dispatcher->getManifoldByIndexInternal(i);
            bool touching = false;
            for (int c = 0; c < manifold->getNumContacts(); ++c) {
                if (manifold->getContactPoint(c).getDistance() <= 0.f) {
                    touching = true;
                    break;
                }
            }
            if (!touching) {
                continue;
            }
            const auto *a = static_cast<const btCollisionObject *>(manifold->getBody0());
            const auto *b = static_cast<const btCollisionObject *>(manifold->getBody1());
            const auto *tagA = static_cast<const BulletObjectTag *>(a->getUserPointer());
            const auto *tagB = static_cast<const BulletObjectTag *>(b->getUserPointer());
            if (tagA == nullptr || tagB == nullptr) {
                continue;
            }
            if (tagA->id < tagB->id) {
                pairs.emplace_back(tagA->id, tagB->id);
            } else {
                pairs.emplace_back(tagB->id, tagA->id);
            }
        }
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    }

    void BulletBackendWorld::Step(float fixedDelta)
    {
        if (!dynamicWorld || fixedDelta <= 0.f) {
            return;
        }
        const auto start = std::chrono::high_resolution_clock::now();

        // maxSubSteps = 0: exactly one step of the requested (fixed) size; no wall-clock dependence.
        dynamicWorld->stepSimulation(fixedDelta, 0, fixedDelta);
        SyncTransforms();

        std::vector<std::pair<PhysicsObjectId, PhysicsObjectId>> pairs;
        GatherContactPairs(pairs);

        events.clear();
        const auto makeTrigger = [this](const std::pair<PhysicsObjectId, PhysicsObjectId> &p) {
            const ObjectEntry *a = Resolve(p.first);
            const ObjectEntry *b = Resolve(p.second);
            return (a && a->isTrigger) || (b && b->isTrigger);
        };
        for (const auto &p : pairs) {
            const bool isNew = previousPairs.find(p) == previousPairs.end();
            const bool trigger = makeTrigger(p);
            PhysicsEvent event;
            event.a = p.first;
            event.b = p.second;
            if (trigger) {
                event.type = isNew ? PhysicsEventType::TriggerEnter : PhysicsEventType::ContactStay;
            } else {
                event.type = isNew ? PhysicsEventType::ContactBegin : PhysicsEventType::ContactStay;
            }
            events.push_back(event);
        }
        for (const auto &p : previousPairs) {
            if (pairs.end() == std::find(pairs.begin(), pairs.end(), p)) {
                const bool trigger = makeTrigger(p);
                PhysicsEvent event;
                event.a = p.first;
                event.b = p.second;
                event.type = trigger ? PhysicsEventType::TriggerExit : PhysicsEventType::ContactEnd;
                events.push_back(event);
            }
        }
        previousPairs.clear();
        previousPairs.insert(pairs.begin(), pairs.end());

        stats.stepCount++;
        stats.bodyCount = 0;
        stats.characterCount = 0;
        stats.constraintCount = 0;
        for (const auto &slot : slots) {
            if (!slot) {
                continue;
            }
            switch (slot->kind) {
                case ObjectKind::Body:
                    stats.bodyCount++;
                    if (slot->body && slot->body->isActive()) {
                        stats.activeBodyCount++;
                    }
                    break;
                case ObjectKind::Character: stats.characterCount++; break;
                case ObjectKind::Constraint: stats.constraintCount++; break;
            }
        }
        stats.lastStepMilliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - start).count();
    }

    bool BulletBackendWorld::Raycast(const Vector3 &origin, const Vector3 &direction, float maxDistance,
                                     const PhysicsQueryFilter &filter, PhysicsQueryHit &out) const
    {
        const float length = direction.Length();
        if (!dynamicWorld || maxDistance <= 0.f || length <= 0.f) {
            return false;
        }
        const btVector3 from = ToBullet(origin);
        const btVector3 to   = ToBullet(origin + direction / length * maxDistance);

        struct Callback : btCollisionWorld::ClosestRayResultCallback {
            const PhysicsQueryFilter *filter;
            Callback(const btVector3 &a, const btVector3 &b, const PhysicsQueryFilter *f)
                : btCollisionWorld::ClosestRayResultCallback(a, b), filter(f) {}

            bool needsCollision(btBroadphaseProxy *proxy) const override
            {
                if (!btCollisionWorld::ClosestRayResultCallback::needsCollision(proxy)) {
                    return false;
                }
                const auto *tag = static_cast<const BulletObjectTag *>(
                    static_cast<btCollisionObject *>(proxy->m_clientObject)->getUserPointer());
                if (tag == nullptr) {
                    return false;
                }
                if (filter != nullptr) {
                    if (filter->Ignores(tag->id)) {
                        return false;
                    }
                    if ((tag->filter.group & filter->mask) == 0) {
                        return false;
                    }
                }
                return true;
            }
        } callback(from, to, &filter);

        dynamicWorld->rayTest(from, to, callback);
        if (!callback.hasHit()) {
            return false;
        }
        const auto *tag = static_cast<const BulletObjectTag *>(callback.m_collisionObject->getUserPointer());
        out.object   = tag ? tag->id : INVALID_PHYSICS_OBJECT_ID;
        out.position = FromBullet(callback.m_hitPointWorld);
        out.normal   = FromBullet(callback.m_hitNormalWorld);
        out.distance = (out.position - origin).Length();
        return true;
    }

    bool BulletBackendWorld::Sweep(const ShapeDesc &shape, const Transform &from, const Vector3 &direction,
                                   float maxDistance, const PhysicsQueryFilter &filter, PhysicsQueryHit &out) const
    {
        const float length = direction.Length();
        if (!dynamicWorld || maxDistance <= 0.f || length <= 0.f) {
            return false;
        }
        ObjectEntry scratch;
        auto shapePtr = CookShape(shape, scratch);
        if (!shapePtr) {
            return false;
        }
        const btTransform start = ToBullet(from);
        btTransform end = start;
        end.getOrigin() += ToBullet(direction / length * maxDistance);

        struct Callback : btCollisionWorld::ClosestConvexResultCallback {
            const PhysicsQueryFilter *filter;
            Callback(const btVector3 &a, const btVector3 &b, const PhysicsQueryFilter *f)
                : btCollisionWorld::ClosestConvexResultCallback(a, b), filter(f) {}

            btScalar addSingleResult(btCollisionWorld::LocalConvexResult &result, bool normalInWorldSpace) override
            {
                const auto *tag = result.m_hitCollisionObject
                    ? static_cast<const BulletObjectTag *>(result.m_hitCollisionObject->getUserPointer())
                    : nullptr;
                if (tag != nullptr && filter != nullptr) {
                    if (filter->Ignores(tag->id) || (tag->filter.group & filter->mask) == 0) {
                        return m_closestHitFraction;
                    }
                }
                return btCollisionWorld::ClosestConvexResultCallback::addSingleResult(result, normalInWorldSpace);
            }
        } callback(start.getOrigin(), end.getOrigin(), &filter);

        if (!shapePtr->isConvex()) {
            return false;
        }
        dynamicWorld->convexSweepTest(static_cast<const btConvexShape *>(shapePtr.get()), start, end, callback);
        if (!callback.hasHit()) {
            return false;
        }
        const auto *tag = static_cast<const BulletObjectTag *>(callback.m_hitCollisionObject->getUserPointer());
        out.object   = tag ? tag->id : INVALID_PHYSICS_OBJECT_ID;
        out.position = FromBullet(callback.m_hitPointWorld);
        out.normal   = FromBullet(callback.m_hitNormalWorld);
        out.distance = (out.position - from.translation).Length();
        return true;
    }

    void BulletBackendWorld::Overlap(const ShapeDesc &shape, const Transform &transform,
                                     const PhysicsQueryFilter &filter, std::vector<PhysicsQueryOverlap> &out) const
    {
        out.clear();
        if (!dynamicWorld) {
            return;
        }
        ObjectEntry scratch;
        auto shapePtr = CookShape(shape, scratch);
        if (!shapePtr) {
            return;
        }
        btCollisionObject probe;
        probe.setCollisionShape(shapePtr.get());
        probe.setWorldTransform(ToBullet(transform));

        struct Callback : btCollisionWorld::ContactResultCallback {
            const PhysicsQueryFilter *filter;
            std::vector<PhysicsQueryOverlap> *out;
            Callback(const PhysicsQueryFilter *f, std::vector<PhysicsQueryOverlap> *o) : filter(f), out(o) {}

            btScalar addSingleResult(btManifoldPoint &, const btCollisionObjectWrapper *a, int, int,
                                     const btCollisionObjectWrapper *b, int, int) override
            {
                const btCollisionObject *candidate = a->m_collisionObject;
                if (candidate->getUserPointer() == nullptr) {
                    candidate = b->m_collisionObject;
                }
                const auto *tag = static_cast<const BulletObjectTag *>(candidate->getUserPointer());
                if (tag == nullptr) {
                    return 0.f;
                }
                if (filter != nullptr && (filter->Ignores(tag->id) || (tag->filter.group & filter->mask) == 0)) {
                    return 0.f;
                }
                out->push_back(PhysicsQueryOverlap{tag->id});
                return 0.f;
            }
        } callback(&filter, &out);

        dynamicWorld->contactTest(&probe, callback);

        std::sort(out.begin(), out.end(), [](const PhysicsQueryOverlap &a, const PhysicsQueryOverlap &b) {
            return a.object < b.object;
        });
        out.erase(std::unique(out.begin(), out.end(),
            [](const PhysicsQueryOverlap &a, const PhysicsQueryOverlap &b) { return a.object == b.object; }),
            out.end());
    }

    void BulletBackendWorld::DrainEvents(std::vector<PhysicsEvent> &out)
    {
        out.insert(out.end(), events.begin(), events.end());
        events.clear();
    }

    void BulletBackendWorld::CollectDebugGeometry(uint32_t categories, PhysicsDebugGeometry &out) const
    {
        if (!dynamicWorld || categories == 0) {
            return;
        }
        CaptureDebugDraw drawer(out);
        int mode = 0;
        if ((categories & PhysicsDebugCategory::Shapes) != 0) {
            mode |= btIDebugDraw::DBG_DrawWireframe;
        }
        if ((categories & PhysicsDebugCategory::AABBs) != 0) {
            mode |= btIDebugDraw::DBG_DrawAabb;
        }
        if ((categories & PhysicsDebugCategory::Contacts) != 0) {
            mode |= btIDebugDraw::DBG_DrawContactPoints;
        }
        drawer.setDebugMode(mode);

        auto *world = const_cast<btDiscreteDynamicsWorld *>(dynamicWorld.get());
        btIDebugDraw *previous = world->getDebugDrawer();
        world->setDebugDrawer(&drawer);
        world->debugDrawWorld();
        world->setDebugDrawer(previous);
    }

    PhysicsWorldStats BulletBackendWorld::GetStats() const
    {
        return stats;
    }

    bool BulletBackendWorld::CaptureState(PhysicsWorldState &out, PhysicsSnapshotScope scope) const
    {
        out.frame = stats.stepCount;
        out.scope = scope;
        out.bodies.clear();
        for (const auto &slot : slots) {
            if (!slot || slot->kind != ObjectKind::Body || !slot->body) {
                continue;
            }
            if (scope == PhysicsSnapshotScope::DynamicKinematic && slot->body->isStaticObject()) {
                continue;
            }
            PhysicsBodyState state;
            state.id              = slot->id;
            state.transform       = FromBullet(slot->body->getWorldTransform());
            state.linearVelocity  = FromBullet(slot->body->getLinearVelocity());
            state.angularVelocity = FromBullet(slot->body->getAngularVelocity());
            state.asleep          = !slot->body->isActive();
            out.bodies.push_back(state);
        }
        return true;
    }

    bool BulletBackendWorld::RestoreState(const PhysicsWorldState &state)
    {
        for (const auto &body : state.bodies) {
            ObjectEntry *entry = Resolve(body.id);
            if (!entry || !entry->body) {
                return false;
            }
            entry->body->setWorldTransform(ToBullet(body.transform));
            entry->body->setLinearVelocity(ToBullet(body.linearVelocity));
            entry->body->setAngularVelocity(ToBullet(body.angularVelocity));
            if (entry->motion) {
                entry->motion->setWorldTransform(ToBullet(body.transform));
            }
            entry->body->activate(!body.asleep);
            entry->curr = body.transform;
            entry->prev = body.transform;
        }
        previousPairs.clear();
        events.clear();
        return true;
    }

} // namespace sky::phy
