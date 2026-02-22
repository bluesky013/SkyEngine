//
// Created by blues on 2024/7/31.
//

#pragma once

namespace sky {
    class World;
    class NativeWindow;

    class IGizmo {
    public:
        IGizmo() = default;
        virtual ~IGizmo() = default;

        virtual void Init(World &world, NativeWindow* window) = 0;
    };

    class IGizmoFactory {
    public:
        IGizmoFactory() = default;
        virtual ~IGizmoFactory() = default;

        virtual IGizmo* CreateGizmo() = 0;
    };

} // namespace sky
