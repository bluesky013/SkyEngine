//
// Created by Zach Lee on 2021/12/11.
//

#pragma once

#include <framework/serialization/Serializer.h>

namespace sky {

    class JsonSerializer : public Serializer {
    public:
        JsonSerializer() = default;
        ~JsonSerializer() = default;

        void WriteString(const Any&, std::string& output) override;

        void ReadString(Any& any, const std::string& input) override;
    };

}