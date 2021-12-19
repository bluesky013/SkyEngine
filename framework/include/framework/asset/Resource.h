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
        ResourceBase() = default;
        virtual ~ResourceBase() = default;

    private:
        AssetBase* asset = nullptr;
    };

}