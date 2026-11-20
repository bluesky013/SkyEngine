//
// Created by blues on 2026/3/10.
//

#pragma once

#include <shader/shadergraph/ShaderGraphTypes.h>
#include <core/util/Uuid.h>
#include <string>

namespace sky::sg {

    // Unique identifier for a pin on a specific node
    struct SGPinID {
        Uuid     nodeId;
        uint32_t pinIndex = 0;

        bool IsValid() const { return static_cast<bool>(nodeId); }

        bool operator==(const SGPinID& rhs) const
        {
            return nodeId == rhs.nodeId && pinIndex == rhs.pinIndex;
        }

        bool operator!=(const SGPinID& rhs) const
        {
            return !(*this == rhs);
        }
    };

    // A pin (input or output slot) on a shader graph node
    struct SGPin {
        std::string    name;
        SGDataType     type      = SGDataType::FLOAT;
        SGPinDirection direction = SGPinDirection::INPUT;
    };

    // A connection between two pins in the shader graph
    struct SGConnection {
        SGPinID src; // output pin
        SGPinID dst; // input pin

        bool IsValid() const { return src.IsValid() && dst.IsValid(); }
    };

} // namespace sky::sg

