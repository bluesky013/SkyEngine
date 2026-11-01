//
// Created by blues on 2024/11/3.
//

#include <recast/RecastQueryFilter.h>

#include <cmath>

namespace sky::ai {

    RecastQueryFilter::RecastQueryFilter()
    {
        data.setIncludeFlags(0xFFFF);
    }

    void RecastQueryFilter::SetAreaCost(AreaId id, float cost)
    {
        if (id < DT_MAX_AREAS && std::isfinite(cost) && cost >= 1.f) {
            data.setAreaCost(id, cost);
        }
    }

    float RecastQueryFilter::GetAreaCost(AreaId id) const
    {
        return id < DT_MAX_AREAS ? data.getAreaCost(id) : 1.f;
    }

    void RecastQueryFilter::SetIncludeFlags(FilterFlags flags)
    {
        data.setIncludeFlags(flags);
    }

    void RecastQueryFilter::SetExcludeFlags(FilterFlags flags)
    {
        data.setExcludeFlags(flags);
    }

} // namespace sky::ai