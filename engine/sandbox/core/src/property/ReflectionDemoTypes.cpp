#include <editor/core/property/PropertyEditor.h>
#include <editor/core/property/ReflectionDemoTypes.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor::refldemo {
    void RegisterTypes(SerializationContext *context)
    {
        context->Register<Quality>("ReflectionDemoQuality").Enum(Quality::Low, "Low").Enum(Quality::Medium, "Medium").Enum(Quality::High, "High");

        context->Register<Placement>("ReflectionDemoPlacement").Member<&Placement::x>("x").Member<&Placement::y>("y");

        context->Register<DemoObject>("ReflectionDemoObject")
            .Member<&DemoObject::enabled>("enabled")
            .Member<&DemoObject::quality>("quality")
            .Member<&DemoObject::count>("count")
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.0f))
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MAX), Any(100.0f))
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_STEP), Any(5.0f))
            .Member<&DemoObject::exposure>("exposure")
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.0f))
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MAX), Any(2.0f))
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_STEP), Any(0.05f))
            .Member<&DemoObject::name>("name")
            .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Display Name")))
            .Member<&DemoObject::tint>("tint")
            .Property(static_cast<uint32_t>(CommonPropertyKey::EDITOR_KIND), Any(static_cast<int32_t>(PropertyEditorKind::Color)))
            .Member<&DemoObject::direction>("direction")
            .Property(static_cast<uint32_t>(CommonPropertyKey::EDITOR_KIND), Any(static_cast<int32_t>(PropertyEditorKind::Vector)))
            .Member<&DemoObject::transform>("transform")
            .Member<&DemoObject::texture>("texture")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string_view("Texture")))
            .Member<&DemoObject::placement>("placement")
            .Member<&DemoObject::weights>("weights");
    }
} // namespace sky::editor::refldemo
