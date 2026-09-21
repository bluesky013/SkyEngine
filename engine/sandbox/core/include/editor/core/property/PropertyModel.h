//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/command/CommandService.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <vector>

namespace sky::editor {

    // Builds and owns the descriptors for one reflected object.
    //
    // A view renders these descriptors; value edits are routed through the
    // CommandService so they are undoable. The model holds no UI-toolkit or
    // render dependency and can be exercised headlessly.
    class PropertyModel {
    public:
        PropertyModel() = default;
        PropertyModel(void *object, const TypeNode *type);
        explicit PropertyModel(const PropertyObject &object);

        const std::vector<PropertyDescriptor> &GetDescriptors() const { return descriptors; }
        bool IsValid() const { return object != nullptr && type != nullptr; }

        // Executes an undoable edit for a descriptor through the command service.
        bool Edit(const PropertyDescriptor &descriptor, Any value, CommandService &commands) const;

        static std::vector<PropertyDescriptor> BuildDescriptors(void *object, const TypeNode *type);

    private:
        void *object = nullptr;
        const TypeNode *type = nullptr;
        std::vector<PropertyDescriptor> descriptors;
    };

} // namespace sky::editor
