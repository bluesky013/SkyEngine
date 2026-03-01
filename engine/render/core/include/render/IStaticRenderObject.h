//
// Created by Zach Lee on 2026/2/26.
//

#pragma once

#include <core/shapes/Bounds.h>
#include <core/event/Event.h>
#include <render/resource/Mesh.h>

namespace sky {

    class Actor;
    using VisibleID = uint32_t;

    struct IStaticRenderObject {
        virtual ~IStaticRenderObject() = default;

        virtual void SetObjectID(VisibleID id) = 0;
        virtual BoundingBoxSphere GetWorldBounds() const = 0;
        virtual Matrix4 GetWorldTransform() const = 0;
        virtual RDMeshPtr GetMesh() const = 0;
    };

    struct IStaticRenderObjectGather : EventTraits {
        using KeyType   = Actor*;
        using MutexType = void;

        virtual void Gather(std::vector<IStaticRenderObject*> &outObjects) = 0;
    };
    using StaticRenderObjectGather = Event<IStaticRenderObjectGather>;
} // namespace sky