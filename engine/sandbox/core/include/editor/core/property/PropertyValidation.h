//
// Created on 2026/10/04.
//

#pragma once

#include <editor/core/property/PropertyEditor.h>
#include <string>

namespace sky::editor {

    // True if a typed character may be appended to a field of this kind
    // (input filter while editing); e.g. numeric fields reject letters.
    bool IsInputCharAllowed(PropertyEditorKind kind, char c);

    // Number of fractional digits to display for a numeric control (derived
    // from its step, or the kind).
    int DecimalsForControl(const EditorControl &control);

    // Parses text matching the member type, then applies the control's
    // constraints (integer-only, range clamp, step quantize). ok=false and an
    // empty Any if the text is not a valid value for the type.
    Any ParseValueText(const TypeInfoRT *type, const EditorControl &control, const std::string &text, bool &ok);

    // Clamps and step-quantizes an existing numeric value to the control's range.
    Any ConstrainValue(const TypeInfoRT *type, const EditorControl &control, const Any &value);

    // Formats a member's current value for display (enum name, bool, number,
    // string), using the control's decimals for numerics.
    std::string FormatPropertyValue(const PropertyDescriptor &descriptor, const EditorControl &control);

} // namespace sky::editor
