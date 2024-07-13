//
// Created by blues on 2024/7/7.
//

#include <editor/framework/ReflectedObjectWidget.h>
#include <core/math/MathUtil.h>
#include <framework/serialization/SerializationUtil.h>

namespace sky::editor {

    ReflectedObjectWidget::ReflectedObjectWidget(void *obj, const TypeNode *node, QWidget *parent)
        : QWidget(parent)
        , object(obj)
        , typeNode(node)
    {
        auto *layout = new QFormLayout(this);
        layout->setLabelAlignment(Qt::AlignLeft);
        layout->setFormAlignment(Qt::AlignCenter);
        setLayout(layout);

        for (const auto &[name, memberInfo] : typeNode->members) {
            ReflectedMemberWidget *widget = nullptr;
            if (memberInfo.info->staticInfo->isEnum) {
                widget = new PropertyEnum(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<bool>::RegisteredId()) {
                widget = new PropertyBool(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<uint64_t>::RegisteredId()) {
                widget = new PropertyScalar<uint64_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<uint32_t>::RegisteredId()) {
                widget = new PropertyScalar<uint32_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<uint16_t>::RegisteredId()) {
                widget = new PropertyScalar<uint16_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<uint8_t>::RegisteredId()) {
                widget = new PropertyScalar<uint8_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<int64_t>::RegisteredId()) {
                widget = new PropertyScalar<int64_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<int32_t>::RegisteredId()) {
                widget = new PropertyScalar<int32_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<int16_t>::RegisteredId()) {
                widget = new PropertyScalar<int16_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<int8_t>::RegisteredId()) {
                widget = new PropertyScalar<int8_t>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<float>::RegisteredId()) {
                widget = new PropertyScalar<float>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<double>::RegisteredId()) {
                widget = new PropertyScalar<double>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Vector2>::RegisteredId()) {
                widget = new PropertyVec<2>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Vector3>::RegisteredId()) {
                widget = new PropertyVec<3>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Vector4>::RegisteredId()) {
                widget = new PropertyVec<4>(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Color>::RegisteredId()) {
                widget = new PropertyColor(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<Uuid>::RegisteredId()) {
                widget = new PropertyUuid(object, &memberInfo, this);
            } else if (memberInfo.info->registeredId == TypeInfo<std::string>::RegisteredId()) {
            }

            if (widget != nullptr) {
                layout->addRow(name.data(), widget);
            }
        }
    }

    void ReflectedObjectWidget::Refresh()
    {
    }

} // namespace sky::editor