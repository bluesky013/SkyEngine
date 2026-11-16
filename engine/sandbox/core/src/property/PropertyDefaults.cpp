//
// Created on 2026/10/07.
//

#include <editor/core/property/PropertyDefaults.h>

#include <core/type/Rtti.h>

namespace sky::editor {

    DefaultPropertyObject MakeDefaultObject(const sky::TypeInfoRT *type)
    {
        if (type == nullptr || type->newFunc == nullptr || type->deleteFunc == nullptr) {
            return {nullptr, nullptr};
        }
        return {type->newFunc(), type->deleteFunc};
    }

} // namespace sky::editor
