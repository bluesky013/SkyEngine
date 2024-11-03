//
// Created by blues on 2024/11/3.
//

#include <recast/RecastQueryFilter.h>

namespace sky::ai {

    RecastQueryFilter::RecastQueryFilter()
    {
        data.setIncludeFlags(0xFFFF);
    }

    void RecastQueryFilter::SetAreaCost(AreaId id, float cost)
    {
        data.setAreaCost(id, cost);
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