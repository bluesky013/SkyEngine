#pragma once

#include <aurora/renderer/PreparedFrameTypes.h>

namespace sky::aurora {

    class PreparePlanBuilder;
    class FrameGraphBuildContext;

    class IScenePrepareFeature {
    public:
        virtual ~IScenePrepareFeature()                                = default;
        virtual Name GetName() const                                   = 0;
        virtual void RegisterScenePrepare(PreparePlanBuilder &builder) = 0;
        virtual void RegisterViewPrepare(PreparePlanBuilder &builder)  = 0;
    };

    // Registration/removal is coordinator-owned; unload waits for all plan CPU/GPU borrowers.
    class IFrameGraphFeature {
    public:
        virtual ~IFrameGraphFeature() = default;
        virtual Name GetName() const  = 0;
        // Coordinator-only; context borrows expire on return. No queue submission or live ECS mutation.
        virtual void BuildView(FrameGraphBuildContext &context, const PreparedViewData &view)          = 0;
        virtual void BuildFrame(FrameGraphBuildContext &context, const FrameExtensionPayload &payload) = 0;
    };

} // namespace sky::aurora
