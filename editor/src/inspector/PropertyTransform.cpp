//
// Created by Zach Lee on 2021/12/16.
//

#include <editor/inspector/PropertyTransform.h>
#include <editor/inspector/PropertyVectorX.h>
#include <QVBoxLayout>
#include <core/math/Transform.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor {

    PropertyTransform::PropertyTransform(QWidget* parent) : PropertyWidget(parent)
    {
        auto layout = new QVBoxLayout(this);
        pos = new PropertyVec<3>(this);
        scale = new PropertyVec<3>(this);
        rotation = new PropertyVec<4>(this);

        layout->addWidget(pos);
        layout->addWidget(scale);
        layout->addWidget(rotation);
    }

    void PropertyTransform::SetInstance(void* instance, const QString&, const TypeMemberNode& memberNode)
    {
        PropertyWidget::SetInstance(instance, "", memberNode);
        // TODO
        Any any = memberNode.getterFn(instance, false);
        Transform* trans = any.GetAs<Transform>();
//        Transform* trans = *static_cast<Transform**>(any.Data());

        auto info = TypeInfoObj<Transform>::Get()->RtInfo();
        auto node = GetTypeNode(info);
        if (node == nullptr) {
            return;
        }

        {
            auto iter = node->members.find("pos");
            static_cast<PropertyVec<3>*>(pos)->SetInstance(trans, iter->first.data(), iter->second);
        }

        {
            auto iter = node->members.find("scale");
            static_cast<PropertyVec<3>*>(scale)->SetInstance(trans, iter->first.data(), iter->second);
        }

        {
            auto iter = node->members.find("rotation");
            static_cast<PropertyVec<4>*>(rotation)->SetInstance(trans, iter->first.data(), iter->second);
        }
    }

}