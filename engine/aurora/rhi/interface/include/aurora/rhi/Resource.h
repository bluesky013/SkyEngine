//
// Created by Zach Lee on 2026/3/30.
//

#pragma once


namespace sky::aurora {

    class IDelayReleaseResource {
    public:
        IDelayReleaseResource() = default;
        virtual ~IDelayReleaseResource() = default;
    };

} // namespace sky::aurora