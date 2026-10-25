#pragma once

#include <core/math/Color.h>
#include <core/math/Transform.h>
#include <core/math/Vector3.h>
#include <core/util/Uuid.h>
#include <cstdint>
#include <string>
#include <vector>

namespace sky {
    class SerializationContext;
}

namespace sky::editor::refldemo {
    enum class Quality { Low = 0, Medium = 1, High = 2 };

    struct Placement {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct DemoObject {
        bool               enabled  = true;
        Quality            quality  = Quality::Medium;
        int32_t            count    = 10;
        float              exposure = 1.0f;
        std::string        name     = "Sky";
        Color              tint{0.9f, 0.5f, 0.2f, 1.0f};
        Vector3            direction{0.0f, 1.0f, 0.0f};
        Transform          transform;
        Uuid               texture;
        Placement          placement{1.0f, 2.0f};
        std::vector<float> weights{1.0f, 2.0f, 3.0f};
    };

    void RegisterTypes(SerializationContext *context);
} // namespace sky::editor::refldemo
