//
// Created on 2026/10/04.
//

#include <editor/core/property/PropertyEditor.h>
#include <core/type/TypeInfo.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor {

    namespace {

        const Any *FindProperty(const serialize::TypeMemberNode &member, CommonPropertyKey key)
        {
            const auto iter = member.properties.find(static_cast<uint32_t>(key));
            return iter == member.properties.end() ? nullptr : &iter->second;
        }

        bool ReadBool(const serialize::TypeMemberNode &member, CommonPropertyKey key, bool &out)
        {
            const Any *any = FindProperty(member, key);
            if (any == nullptr) {
                return false;
            }
            if (const bool *v = any->GetAsConst<bool>()) {
                out = *v;
                return true;
            }
            return false;
        }

        bool ReadDouble(const serialize::TypeMemberNode &member, CommonPropertyKey key, double &out)
        {
            const Any *any = FindProperty(member, key);
            if (any == nullptr) {
                return false;
            }
            if (const double *d = any->GetAsConst<double>()) {
                out = *d;
                return true;
            }
            if (const float *f = any->GetAsConst<float>()) {
                out = static_cast<double>(*f);
                return true;
            }
            if (const int32_t *i = any->GetAsConst<int32_t>()) {
                out = static_cast<double>(*i);
                return true;
            }
            return false;
        }

        bool ReadInt(const serialize::TypeMemberNode &member, CommonPropertyKey key, int32_t &out)
        {
            const Any *any = FindProperty(member, key);
            if (any == nullptr) {
                return false;
            }
            if (const int32_t *i = any->GetAsConst<int32_t>()) {
                out = *i;
                return true;
            }
            return false;
        }

        bool ReadString(const serialize::TypeMemberNode &member, CommonPropertyKey key, std::string &out)
        {
            const Any *any = FindProperty(member, key);
            if (any == nullptr) {
                return false;
            }
            if (const std::string *s = any->GetAsConst<std::string>()) {
                out = *s;
                return true;
            }
            if (const std::string_view *sv = any->GetAsConst<std::string_view>()) {
                out = std::string(*sv);
                return true;
            }
            return false;
        }

    } // namespace

    PropertyAttributes ReadPropertyAttributes(const serialize::TypeMemberNode &member)
    {
        PropertyAttributes attrs;

        bool b = false;
        if (ReadBool(member, CommonPropertyKey::VISIBLE, b)) {
            attrs.visible = b;
        }
        if (ReadBool(member, CommonPropertyKey::READONLY, b)) {
            attrs.readOnly = b;
        }
        if (ReadBool(member, CommonPropertyKey::MULTILINE, b)) {
            attrs.multiline = b;
        }
        if (ReadBool(member, CommonPropertyKey::ENUM_FLAGS, b)) {
            attrs.enumFlags = b;
        }

        ReadString(member, CommonPropertyKey::LABEL, attrs.label);
        ReadString(member, CommonPropertyKey::TOOLTIP, attrs.tooltip);
        ReadString(member, CommonPropertyKey::CATEGORY, attrs.category);
        ReadString(member, CommonPropertyKey::EDITOR_HINT, attrs.editorHint);
        ReadString(member, CommonPropertyKey::COLOR_SPACE, attrs.colorSpace);
        ReadString(member, CommonPropertyKey::ASSET_TYPE, attrs.assetType);

        int32_t order = 0;
        if (ReadInt(member, CommonPropertyKey::ORDER, order)) {
            attrs.order = order;
        }

        int32_t kind = 0;
        if (ReadInt(member, CommonPropertyKey::EDITOR_KIND, kind)) {
            attrs.hasKindOverride = true;
            attrs.kindOverride = static_cast<PropertyEditorKind>(kind);
        }

        double minValue = 0.0;
        double maxValue = 0.0;
        double stepValue = 0.0;
        const bool hasMin = ReadDouble(member, CommonPropertyKey::RANGE_MIN, minValue);
        const bool hasMax = ReadDouble(member, CommonPropertyKey::RANGE_MAX, maxValue);
        const bool hasStep = ReadDouble(member, CommonPropertyKey::RANGE_STEP, stepValue);
        if (hasMin || hasMax || hasStep) {
            attrs.hasRange = true;
            attrs.rangeMin = minValue;
            attrs.rangeMax = hasMax ? maxValue : 1.0;
            attrs.rangeStep = stepValue;
        }

        return attrs;
    }

    void PropertyEditorRegistry::RegisterType(const Uuid &typeId, PropertyEditorKind kind)
    {
        typeKinds[typeId] = kind;
    }

    bool PropertyEditorRegistry::HasType(const Uuid &typeId) const
    {
        return typeKinds.find(typeId) != typeKinds.end();
    }

    void PropertyEditorRegistry::RegisterMemberAppearance(const Uuid &typeId, const std::string &memberName,
                                                          const MemberAppearance &appearance)
    {
        memberAppearances[typeId][memberName] = appearance;
    }

    const MemberAppearance *PropertyEditorRegistry::GetMemberAppearance(const Uuid &typeId,
                                                                        const std::string &memberName) const
    {
        const auto typeIter = memberAppearances.find(typeId);
        if (typeIter == memberAppearances.end()) {
            return nullptr;
        }
        const auto memberIter = typeIter->second.find(memberName);
        return memberIter == typeIter->second.end() ? nullptr : &memberIter->second;
    }

    EditorControl PropertyEditorRegistry::Resolve(const PropertyDescriptor &descriptor) const
    {
        EditorControl control;
        if (!descriptor.IsValid()) {
            return control;
        }

        const serialize::TypeMemberNode *member = descriptor.GetMember();
        const PropertyAttributes attrs = ReadPropertyAttributes(*member);

        control.readOnly = attrs.readOnly;
        control.multiline = attrs.multiline;
        control.hasRange = attrs.hasRange;
        control.rangeMin = attrs.rangeMin;
        control.rangeMax = attrs.rangeMax;
        control.rangeStep = attrs.rangeStep;
        control.editorHint = attrs.editorHint;
        control.assetType = attrs.assetType;
        control.label = attrs.label;

        if (attrs.hasKindOverride) {
            control.kind = attrs.kindOverride;
        } else if (!attrs.assetType.empty()) {
            control.kind = PropertyEditorKind::Asset;
        } else {
            const TypeInfoRT *type = descriptor.GetType();
            const auto iter = (type != nullptr) ? typeKinds.find(type->registeredId) : typeKinds.end();
            if (iter != typeKinds.end()) {
                control.kind = iter->second;
            } else {
                control.kind = InferKind(descriptor);
            }
        }

        if (control.kind == PropertyEditorKind::Enum) {
            if (const TypeNode *node = GetTypeNode(descriptor.GetType())) {
                for (const auto &entry : node->enums) {
                    control.enumValues.push_back(static_cast<int64_t>(entry.first));
                    control.enumNames.emplace_back(entry.second);
                }
            }
        }

        if (control.kind == PropertyEditorKind::Color || control.kind == PropertyEditorKind::Vector) {
            if (const TypeNode *node = descriptor.GetStructType()) {
                control.componentCount = static_cast<uint32_t>(node->members.size());
            }
        }

        return control;
    }

    PropertyEditorKind PropertyEditorRegistry::InferKind(const PropertyDescriptor &descriptor)
    {
        const TypeInfoRT *type = descriptor.GetType();
        if (type == nullptr || type->staticInfo == nullptr) {
            return PropertyEditorKind::Unknown;
        }

        if (descriptor.IsSequence()) {
            return PropertyEditorKind::Sequence;
        }
        if (type->staticInfo->isEnum) {
            return PropertyEditorKind::Enum;
        }
        if (type->registeredId == TypeInfo<bool>::RegisteredId()) {
            return PropertyEditorKind::Bool;
        }
        if (type->registeredId == TypeInfo<std::string>::RegisteredId()) {
            return PropertyEditorKind::String;
        }
        if (type->staticInfo->isInteger) {
            return PropertyEditorKind::Integer;
        }
        if (type->staticInfo->isFloatingPoint) {
            return PropertyEditorKind::Float;
        }
        if (descriptor.IsStruct()) {
            return PropertyEditorKind::Struct;
        }
        return PropertyEditorKind::Unknown;
    }

} // namespace sky::editor
