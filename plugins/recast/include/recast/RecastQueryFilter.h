//
// Created by blues on 2024/11/3.
//

#pragma once

#include <navigation/NaviQueryFilter.h>
#include <DetourNavMeshQuery.h>

namespace sky::ai {

    class RecastQueryFilter : public NaviQueryFilter {
    public:
        RecastQueryFilter();
        ~RecastQueryFilter() override = default;

        const dtQueryFilter* GetFilter() const { return &data; }

    private:
        void SetAreaCost(AreaId id, float cost) override;
        void SetIncludeFlags(FilterFlags flags) override;
        void SetExcludeFlags(FilterFlags flags) override;

        dtQueryFilter data = {};
    };

} // namespace sky::ai
