//
// Created by Zach Lee on 2021/12/11.
//

#pragma once

#include <framework/serialization/Any.h>
#include <string>

namespace sky {

    class Serializer {
    public:
        Serializer() = default;
        virtual ~Serializer() = default;

        virtual void WriteString(const Any&, std::string& output) = 0;

        virtual void ReadString(Any& any, const std::string& input) = 0;
    };

}
