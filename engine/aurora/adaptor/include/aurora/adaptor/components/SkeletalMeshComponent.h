#pragma once

#include <aurora/adaptor/components/AuroraComponentData.h>
#include <aurora/adaptor/renderer/SceneComponentEvent.h>

namespace sky::aurora {
    class SkeletalMeshComponent final : public SceneComponentAdaptor<SkeletalMeshComponentData> {
    public:
        COMPONENT_RUNTIME_INFO(SkeletalMeshComponent)
        static void Reflect(SerializationContext *context);
        void        SetConfig(const SkeletalMeshComponentData &value)
        {
            data = value;
        }
        void SetMeshUuid(const Uuid &value)
        {
            data.mesh = value;
        }
        const Uuid &GetMeshUuid() const
        {
            return data.mesh;
        }
        void SetSkinUuid(const Uuid &value)
        {
            data.skin = value;
        }
        const Uuid &GetSkinUuid() const
        {
            return data.skin;
        }
        void SetSkeletonUuid(const Uuid &value)
        {
            data.skeleton = value;
        }
        const Uuid &GetSkeletonUuid() const
        {
            return data.skeleton;
        }
        void SetMaterialUuid(const Uuid &value)
        {
            data.material = value;
        }
        const Uuid &GetMaterialUuid() const
        {
            return data.material;
        }
        void SetPoseMode(SkeletalPoseMode value)
        {
            data.pose = value;
        }
        SkeletalPoseMode GetPoseMode() const
        {
            return data.pose;
        }
        void SetCastShadow(bool value)
        {
            data.castShadow = value;
        }
        bool GetCastShadow() const
        {
            return data.castShadow;
        }
        void SetReceiveShadow(bool value)
        {
            data.receiveShadow = value;
        }
        bool GetReceiveShadow() const
        {
            return data.receiveShadow;
        }
    };
} // namespace sky::aurora
