//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

#include <core/math/Transform.h>
#include <engine/world/Component.h>
#include <vector>

namespace sky {

    class TransformComponent : public Component {
    public:
        TransformComponent() = default;
        ~TransformComponent();

        TYPE_RTTI_WITH_VT(TransformComponent)

        static void Reflect();

        void SetParent(TransformComponent*);

        TransformComponent* GetParent() const;

        const std::vector<TransformComponent*>& GetChildren() const;

        void SetWorldTranslation(const Vector3& translation);
        void SetWorldRotation(const Quaternion& rotation);
        void SetWorldScale(const Vector3& scale);

        void SetLocalTranslation(const Vector3& translation);
        void SetLocalRotation(const Quaternion& rotation);
        void SetLocalScale(const Vector3& scale);

        const Transform& GetLocal() const;
        const Transform& GetWorld() const;

        void Print();
    private:
        static void PrintChild(TransformComponent& comp, std::string str);

        void TransformChanged();

        void UpdateLocal();

        void UpdateWorld();

        const Transform& GetParentTransform() const;

        Transform local;
        Transform world;
        bool suppressWorldChange = false;
        TransformComponent* parent = nullptr;
        std::vector<TransformComponent*> children;
    };

}