//
// Created by blues on 2024/11/3.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <memory>

namespace sky::ai {

    using AreaId      = uint8_t;
    using FilterFlags = uint16_t;

    class NaviQueryFilter : public RefObject {
    public:
        // Serialize edits with synchronous queries and asynchronous submission; queued requests use their snapshot.
        NaviQueryFilter()           = default;
        ~NaviQueryFilter() override = default;

        virtual void        SetAreaCost(AreaId id, float cost) = 0;
        virtual void        SetIncludeFlags(FilterFlags flags) = 0;
        virtual void        SetExcludeFlags(FilterFlags flags) = 0;
        virtual float       GetAreaCost(AreaId id) const       = 0;
        virtual FilterFlags GetIncludeFlags() const            = 0;
        virtual FilterFlags GetExcludeFlags() const            = 0;

        void SetCodeLease(std::shared_ptr<const void> value)
        {
            codeLease = std::move(value);
        }

    private:
        std::shared_ptr<const void> codeLease;
    };
    using NaviQueryFilterPtr = CounterPtr<NaviQueryFilter>;

} // namespace sky::ai
