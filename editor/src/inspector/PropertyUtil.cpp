//
// Created by Zach Lee on 2021/12/16.
//

#include <editor/inspector/PropertyUtil.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor::util {

    bool CheckProperty(const PropertyMap &properties, CommonPropertyKey key, bool dft)
    {
        auto iter = properties.find(key);
        if (iter != properties.end()) {
            const auto *val = iter->second.GetAsConst<bool>();
            return val != nullptr ? *val : dft;
        }
        return true;
    }
} // namespace sky::editor::util
