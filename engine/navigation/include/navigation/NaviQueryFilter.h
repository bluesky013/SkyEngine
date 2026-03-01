//
// Created by blues on 2024/11/3.
//

#pragma once

#include <core/template/ReferenceObject.h>

namespace sky::ai {

    using AreaId = uint8_t;
    using FilterFlags = uint16_t;

    class NaviQueryFilter : public RefObject {
    public:
        NaviQueryFilter() = default;
        ~NaviQueryFilter() override = default;

        virtual void SetAreaCost(AreaId id, float cost) = 0;

        virtual void SetIncludeFlags(FilterFlags flags) = 0;
        virtual void SetExcludeFlags(FilterFlags flags) = 0;
    };
    using NaviQueryFilterPtr = CounterPtr<NaviQueryFilter>;

} // namespace sky::ai
