//
// Cook orchestration split out of EditorAssetCatalog. Editor triggers only *enqueue* requests (deduped);
// the editor tick drains the queue on the main thread (see editor-cook-async). Process-wide (cross-DLL).
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>

#include <mutex>
#include <unordered_set>

namespace sky::editor {

    class AssetCookService : public sky::Singleton<AssetCookService> {
        friend class sky::Singleton<AssetCookService>;

    public:
        // Enqueue a cook request for one asset (deduped per asset). Event-safe: only inserts, so it
        // may be called from a pointer/key/text handler without doing any cook work.
        void RequestCook(const Uuid &uuid);

        // Enqueue a cook for every cookable source (coalesced into one drain).
        void RequestCookAll();

        // Main thread, once per frame: run the queued requests (BeginCook + BuildAllTargets per
        // asset, across its resolved targets).
        void Drain();

    private:
        void CookOne(const Uuid &uuid);
        void CookAll();

        std::mutex                 mutex;
        std::unordered_set<Uuid>   pending;
        bool                       cookAllPending = false;
    };

} // namespace sky::editor
