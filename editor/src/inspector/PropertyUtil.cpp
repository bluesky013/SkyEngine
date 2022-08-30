//
// Created by Zach Lee on 2021/12/16.
//

#include <core/math/Transform.h>
#include <core/math/Vector.h>
#include <editor/inspector/PropertyTransform.h>
#include <editor/inspector/PropertyUtil.h>
#include <editor/inspector/PropertyVectorX.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor::util {

    bool IsVisible(const TypeMemberNode &member)
    {
        auto iter = member.properties.find(UI_PROP_VISIBLE);
        if (iter != member.properties.end()) {
            auto val = iter->second.GetAsConst<bool>();
            return val == nullptr ? false : *val;
        }
        return true;
    }

    PropertyWidget *CreateByTypeMemberInfo(const TypeMemberNode &member, QWidget *parent)
    {
        if (member.info == TypeInfoObj<float>::Get()->RtInfo()) {
            return new PropertyScalar<float>(parent);
        } else if (member.info == TypeInfoObj<double>::Get()->RtInfo()) {
            return new PropertyScalar<double>(parent);
        } else if (member.info == TypeInfoObj<int32_t>::Get()->RtInfo()) {
            return new PropertyScalar<int32_t>(parent);
        } else if (member.info == TypeInfoObj<uint32_t>::Get()->RtInfo()) {
            return new PropertyScalar<uint32_t>(parent);
        } else if (member.info == TypeInfoObj<Transform>::Get()->RtInfo()) {
            return new PropertyTransform(parent);
        }
        return nullptr;
    }
} // namespace sky::editor::util