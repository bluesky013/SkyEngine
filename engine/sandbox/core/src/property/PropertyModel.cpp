//
// Created on 2026/09/21.
//

#include <editor/core/property/PropertyModel.h>
#include <string>
#include <utility>

namespace sky::editor {

    std::vector<PropertyDescriptor> PropertyModel::BuildDescriptors(void *object, const TypeNode *type)
    {
        std::vector<PropertyDescriptor> result;
        if (object == nullptr || type == nullptr) {
            return result;
        }

        const std::string category = type->info != nullptr ? std::string(type->info->name) : std::string();
        result.reserve(type->members.size());
        for (const auto &[memberName, memberNode] : type->members) {
            result.emplace_back(object, &memberNode, std::string(memberName), category);
        }
        return result;
    }

    PropertyModel::PropertyModel(void *object, const TypeNode *type)
        : object(object), type(type), descriptors(BuildDescriptors(object, type))
    {
    }

    PropertyModel::PropertyModel(const PropertyObject &object)
        : PropertyModel(object.object, object.type)
    {
    }

    bool PropertyModel::Edit(const PropertyDescriptor &descriptor, Any value, CommandService &commands) const
    {
        if (!descriptor.CanEdit()) {
            return false;
        }
        commands.Execute(descriptor.MakeEditCommand(std::move(value)));
        return true;
    }

} // namespace sky::editor
