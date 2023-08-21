//
// Created by Zach Lee on 2021/12/16.
//

#include <editor/inspector/PropertyUtil.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor::util {

    bool IsVisible(const TypeMemberNode &member)
    {
        auto iter = member.properties.find(UI_PROP_VISIBLE);
        if (iter != member.properties.end()) {
            const auto *val = iter->second.GetAsConst<bool>();
            return val != nullptr && *val;
        }
        return true;
    }
} // namespace sky::editor::util
