//
// Created by blues on 2024/3/17.
//

#pragma once

#include <string>

namespace sky::editor {

    class IWidget {
    public:
        explicit IWidget(const std::string &str) : name(str) {}
        virtual ~IWidget() = default;

        const std::string &GetName() const { return name; }
    protected:
        std::string name;
    };

} // namespace sky::editor