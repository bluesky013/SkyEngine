//
// Created on 2026/10/04.
//

#pragma once

#include <core/util/Uuid.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // Selection -> reflected data seam. EditorCore depends on no world type; the
    // host installs an implementation (module) or uses RegisteredPropertySource.
    class IEditorPropertySource {
    public:
        virtual ~IEditorPropertySource()                                         = default;
        virtual std::vector<PropertyObject> Resolve(const Uuid &selection) const = 0;
    };

    // Default, world-free source: extensions publish inspectable objects by key.
    class RegisteredPropertySource : public IEditorPropertySource {
    public:
        void Add(const Uuid &id, const PropertyObject &object)
        {
            objects[id].push_back(object);
        }
        void Remove(const Uuid &id)
        {
            objects.erase(id);
        }
        void Clear()
        {
            objects.clear();
        }

        std::vector<PropertyObject> Resolve(const Uuid &selection) const override
        {
            const auto iter = objects.find(selection);
            return iter == objects.end() ? std::vector<PropertyObject>{} : iter->second;
        }

    private:
        std::unordered_map<Uuid, std::vector<PropertyObject>> objects;
    };

} // namespace sky::editor
