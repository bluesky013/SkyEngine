//
// Created by blues on 2026/10/3.
//

#pragma once

#include <functional>
#include <string>
#include <core/util/Uuid.h>
#include <framework/asset/AssetCommon.h>

namespace sky {

    struct CookJob {
        Uuid        uuid;
        std::string target;
        std::string path; // logical source path; used by the out-of-process runner
    };

    // Cook dispatch seam. The loading layer depends only on this and IAssetEvent, so an
    // in-process and an out-of-process cook are indistinguishable: both raise
    // IAssetEvent::OnAssetBuildFinished and invoke the completion handler (D5/D7).
    class ICookRunner {
    public:
        using CookCompletion = std::function<void(const AssetBuildResult &result)>;

        virtual ~ICookRunner() = default;

        // Called from the completing thread for every finished cook (success or failure).
        virtual void SetCompletion(CookCompletion handler) = 0;

        virtual bool Request(const CookJob &job) = 0;
        virtual void Drain() = 0;
    };

} // namespace sky
