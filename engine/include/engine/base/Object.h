//
// Created by Zach Lee on 2023/1/15.
//

#pragma once

#include <core/util/Uuid.h>

namespace sky {

    class Object {
    public:
        Object() = default;
        ~Object() = default;

        void SetUuid(const Uuid &id) { uuid = id; }
        const Uuid &GetUuid() const { return uuid; }

    protected:
        Uuid uuid;
    };

}