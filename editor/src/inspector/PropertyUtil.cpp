//
// Created by Zach Lee on 2021/12/16.
//

#include <editor/inspector/PropertyUtil.h>
#include <editor/inspector/PropertyVectorX.h>
#include <editor/inspector/PropertyTransform.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/PropertyCommon.h>
#include <core/math/Vector.h>
#include <core/math/Transform.h>

namespace sky::editor::util {

    bool IsVisible(const TypeMemberNode& member)
    {
        auto iter = member.properties.find(UI_PROP_VISIBLE);
        if (iter != member.properties.end()) {
            auto val = iter->second.GetAsConst<bool>();
            return val == nullptr ? false : *val;
        }
        return true;
    }

    PropertyWidget* CreateByTypeMemberInfo(const TypeMemberNode& member, QWidget* parent)
    {
        if (member.info == TypeInfoObj<float>::Get()->RtInfo() ||
            member.info == TypeInfoObj<double>::Get()->RtInfo()) {
            return new PropertyVec<1>(parent);
        } else if (member.info == TypeInfoObj<Transform>::Get()->RtInfo()) {
            return new PropertyTransform(parent);
        }
        return nullptr;
    }
}