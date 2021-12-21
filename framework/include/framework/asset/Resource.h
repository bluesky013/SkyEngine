//
// Created by Zach Lee on 2021/12/19.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/template/ReferenceObject.h>

namespace sky {

    class AssetBase;

    class ResourceBase : public RefObject<ResourceBase> {
    public:
        ResourceBase(const Uuid& id) : uuid(id) {}
        virtual ~ResourceBase() = default;

    protected:
        friend class ResourceManager;

        void OnExpire() override;

        AssetBase* asset = nullptr;
        Uuid uuid;
    };

    using ResourceInstance = CounterPtr<ResourceBase>;

}