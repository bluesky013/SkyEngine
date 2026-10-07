//
// Created on 2026/10/07.
//

#include <editor/core/property/PropertyDefaults.h>

#include <core/type/Rtti.h>

namespace sky::editor {

    Any MakeDefaultValue(const sky::TypeInfoRT *type)
    {
        if (type == nullptr || type->newFunc == nullptr) {
            return {};
        }
        void *instance = type->newFunc();
        if (instance == nullptr) {
            return {};
        }
        Any value = Any::Create(type, instance);
        if (type->deleteFunc != nullptr) {
            type->deleteFunc(instance);
        }
        return value;
    }

} // namespace sky::editor
