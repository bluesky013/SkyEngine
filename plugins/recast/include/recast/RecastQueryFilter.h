//
// Created by blues on 2024/11/3.
//

#pragma once

#include <DetourNavMeshQuery.h>
#include <navigation/NaviQueryFilter.h>

namespace sky::ai {

    class RecastQueryFilter : public NaviQueryFilter {
    public:
        RecastQueryFilter();
        ~RecastQueryFilter() override = default;

        const dtQueryFilter *GetFilter() const
        {
            return &data;
        }
        float       GetAreaCost(AreaId id) const override;
        FilterFlags GetIncludeFlags() const override
        {
            return data.getIncludeFlags();
        }
        FilterFlags GetExcludeFlags() const override
        {
            return data.getExcludeFlags();
        }

    private:
        void SetAreaCost(AreaId id, float cost) override;
        void SetIncludeFlags(FilterFlags flags) override;
        void SetExcludeFlags(FilterFlags flags) override;

        dtQueryFilter data = {};
    };

} // namespace sky::ai
