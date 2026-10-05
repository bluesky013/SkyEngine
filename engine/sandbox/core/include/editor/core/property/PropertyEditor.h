//
// Created on 2026/10/04.
//

#pragma once

#include <editor/core/property/PropertyDescriptor.h>
#include <core/util/Uuid.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // Toolkit-independent editor control kind, resolved from reflection metadata
    // (and optional attributes) so a view can pick a control without knowing C++.
    enum class PropertyEditorKind : uint8_t {
        Unknown = 0,
        Bool,
        Integer,
        Float,
        String,
        Enum,
        Asset,
        Color,
        Vector,
        Rotation,
        Struct,
        Sequence
    };

    // Resolved control description for one reflected member.
    struct EditorControl {
        PropertyEditorKind kind = PropertyEditorKind::Unknown;
        bool               readOnly = false;
        bool               multiline = false;
        bool               hasRange = false;
        double             rangeMin = 0.0;
        double             rangeMax = 1.0;
        double             rangeStep = 0.0;
        uint32_t           componentCount = 1;
        std::string        label;
        std::string        tooltip;
        std::string        editorHint;
        std::string        assetType;
        std::vector<int64_t>     enumValues;
        std::vector<std::string> enumNames;
    };

    // Per-member UI attributes read from the reflected attribute map.
    struct PropertyAttributes {
        bool        visible = true;
        bool        readOnly = false;
        bool        multiline = false;
        bool        enumFlags = false;
        std::string label;
        std::string tooltip;
        std::string category;
        std::string editorHint;
        std::string assetType;
        std::string colorSpace;
        int32_t     order = 0;
        bool        hasRange = false;
        double      rangeMin = 0.0;
        double      rangeMax = 0.0;
        double      rangeStep = 0.0;
        bool        hasKindOverride = false;
        PropertyEditorKind kindOverride = PropertyEditorKind::Unknown;
    };

    PropertyAttributes ReadPropertyAttributes(const serialize::TypeMemberNode &member);

    // Editor-side appearance override for a reflected member (label / sort order),
    // declared by the editor without touching the data type's registration.
    struct MemberAppearance {
        std::string label;
        int         order = 0;
        bool        hasOrder = false;
    };

    // Maps reflected types/members to editor controls; extensible by modules.
    class PropertyEditorRegistry {
    public:
        PropertyEditorRegistry() = default;
        ~PropertyEditorRegistry() = default;

        PropertyEditorRegistry(const PropertyEditorRegistry &) = delete;
        PropertyEditorRegistry &operator=(const PropertyEditorRegistry &) = delete;

        void RegisterType(const Uuid &typeId, PropertyEditorKind kind);
        bool HasType(const Uuid &typeId) const;

        void RegisterMemberAppearance(const Uuid &typeId, const std::string &memberName,
                                      const MemberAppearance &appearance);
        const MemberAppearance *GetMemberAppearance(const Uuid &typeId, const std::string &memberName) const;

        EditorControl Resolve(const PropertyDescriptor &descriptor) const;

    private:
        static PropertyEditorKind InferKind(const PropertyDescriptor &descriptor);

        std::unordered_map<Uuid, PropertyEditorKind> typeKinds;
        std::unordered_map<Uuid, std::unordered_map<std::string, MemberAppearance>> memberAppearances;
    };

} // namespace sky::editor
