//
// Created by Zach Lee on 2021/12/16.
//

#pragma once
#include <editor/inspector/PropertyWidget.h>

namespace sky {
    struct TypeMemberNode;
} // sky

namespace sky::editor::util {

    bool IsVisible(const TypeMemberNode& member);

} // sky::editor::util
