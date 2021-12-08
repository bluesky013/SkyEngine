//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <framework/environment/Singleton.h>

namespace sky {

    class SerializationContext : public Singleton<SerializationContext> {
    public:

    private:
        friend class Singleton<SerializationContext>;

        SerializationContext() = default;
        ~SerializationContext() = default;
    };

}
