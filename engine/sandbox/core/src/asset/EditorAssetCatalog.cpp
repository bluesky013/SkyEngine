//
// Created on 2026/10/04.
//

#include <editor/core/asset/EditorAssetCatalog.h>

#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetDependencyProvider.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/serialization/SerializationContext.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sky::editor {

    namespace {

        // Splits a display path ("Project/textures/a.tex") into (mount, mount-relative path).
        void SplitDisplay(const std::string &display, std::string &mount, std::string &relative)
        {
            const auto slash = display.find('/');
            if (slash == std::string::npos) {
                mount    = display;
                relative = {};
                return;
            }
            mount    = display.substr(0, slash);
            relative = display.substr(slash + 1);
        }

        std::string JoinDisplay(const std::string &mount, const std::string &relative)
        {
            if (relative.empty()) {
                return mount;
            }
            return mount + "/" + relative;
        }

        // Logical paths use '/' regardless of the host separator.
        std::string NormalizeSlashes(std::string path)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }

        // Immediate parent logical path ("a/b/c.tex" -> "a/b"; "a.tex" -> "").
        std::string ParentPath(const std::string &path)
        {
            const auto slash = path.rfind('/');
            return slash == std::string::npos ? std::string{} : path.substr(0, slash);
        }

        // Writability of a mount by its display name (from the framework mount record).
        bool MountWritable(const std::string &displayName)
        {
            auto *db = sky::AssetDataBase::Get();
            if (db == nullptr) {
                return false;
            }
            for (const auto &mount : db->GetMounts()) {
                if (mount.displayName == displayName) {
                    return mount.writable;
                }
            }
            return false;
        }

        std::string SerializeJson(const rapidjson::Value &value)
        {
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            value.Accept(writer);
            return std::string(buffer.GetString(), buffer.GetSize());
        }

        // Parse a raw cook block into an object document (empty/invalid becomes an empty object).
        void ParseCook(const std::string &cookJson, rapidjson::Document &doc)
        {
            doc.Parse<rapidjson::kParseCommentsFlag>(cookJson.c_str(), cookJson.size());
            if (doc.HasParseError() || !doc.IsObject()) {
                doc.SetObject();
            }
        }

    } // namespace

    // Internal asset-event listener (defined here so the framework event interface stays out of the
    // catalog's public header).
    struct EditorAssetCatalog::BuildListener : public sky::IAssetEvent {
        explicit BuildListener(EditorAssetCatalog *o) : owner(o)
        {
        }

        void OnAssetBuildFinished(const sky::AssetBuildResult &result) override
        {
            owner->OnBuildFinished(result.uuid, result.target, result.retCode == sky::AssetBuildRetCode::SUCCESS, result.error);
        }

        EditorAssetCatalog *owner;
    };

    EditorAssetCatalog::EditorAssetCatalog() : listener(std::make_unique<BuildListener>(this))
    {
    }
    EditorAssetCatalog::~EditorAssetCatalog() = default;

    EditorAssetItem EditorAssetCatalog::MakeItem(const sky::AssetSourceInfo &source) const
    {
        EditorAssetItem item;
        item.uuid = source.uuid;
        item.name = source.name.empty() ? source.path.FileName() : source.name;

        const std::string relative = NormalizeSlashes(source.path.GetStr());

        // Provenance comes from the framework mount record (id -> display name / writable).
        std::string mountName = source.mount;
        bool        writable  = false;
        if (auto *db = sky::AssetDataBase::Get(); db != nullptr) {
            for (const auto &mount : db->GetMounts()) {
                if (mount.id == source.mount) {
                    mountName = mount.displayName;
                    writable  = mount.writable;
                    break;
                }
            }
            db->GetType(source.uuid, item.type);
        }
        item.mount    = mountName;
        item.writable = writable;
        item.path     = JoinDisplay(mountName, relative);

        {
            std::lock_guard lock(stateMutex);
            if (auto iter = cookStates.find(source.uuid); iter != cookStates.end()) {
                item.state = AggregateState(iter->second);
                for (const auto &[bundle, state] : iter->second) {
                    if (state.state == AssetCookState::Failed && !state.error.empty()) {
                        item.error = state.error;
                        break;
                    }
                }
            }
        }
        return item;
    }

    std::vector<EditorAssetItem> EditorAssetCatalog::CollectItems() const
    {
        std::lock_guard lock(itemsMutex);
        const auto      revision = stateRevision.load(std::memory_order_relaxed);
        if (itemsRevision != revision) {
            itemsCache    = BuildItems();
            itemsRevision = revision;
        }
        return itemsCache;
    }

    void EditorAssetCatalog::BumpRevision()
    {
        stateRevision.fetch_add(1, std::memory_order_relaxed);
    }

    std::vector<EditorAssetItem> EditorAssetCatalog::BuildItems() const
    {
        std::vector<EditorAssetItem> items;
        auto                        *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return items;
        }

        db->ForEachSource([&](const sky::AssetSourcePtr &source) {
            if (!source) {
                return;
            }
            // List every registered source. Manifest/sidecar files are never registered, so they
            // do not appear. Type is optional metadata (the editor may not have loaded the builders
            // that derive it), so it is not used to filter the list.
            items.push_back(MakeItem(*source));
        });

        std::sort(items.begin(), items.end(), [](const EditorAssetItem &a, const EditorAssetItem &b) {
            if (a.path != b.path) {
                return a.path < b.path;
            }
            return a.uuid < b.uuid;
        });
        return items;
    }

    std::vector<EditorAssetItem> EditorAssetCatalog::Gather(const std::string &type) const
    {
        std::vector<EditorAssetItem> out;
        for (auto &item : CollectItems()) {
            if (item.type == type) {
                out.push_back(std::move(item));
            }
        }
        return out;
    }

    bool EditorAssetCatalog::GetType(const Uuid &uuid, std::string &outType) const
    {
        auto *db = sky::AssetDataBase::Get();
        return db != nullptr && db->GetType(uuid, outType);
    }

    bool EditorAssetCatalog::GetName(const Uuid &uuid, std::string &outName) const
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return false;
        }
        auto source = db->FindAsset(uuid);
        if (!source) {
            return false;
        }
        outName = source->name.empty() ? source->path.FileName() : source->name;
        return true;
    }

    std::vector<EditorAssetFolder> EditorAssetCatalog::GetRoots()
    {
        // The synthetic root's children are the mount folders.
        return ListFolder({}).folders;
    }

    EditorAssetFolder EditorAssetCatalog::GetTree() const
    {
        EditorAssetFolder root;
        const auto        items = CollectItems();

        // Phase 1: one node per folder path; attach each item to its containing folder.
        std::map<std::string, EditorAssetFolder> nodes;
        const auto                               ensure = [&nodes](const std::string &path) {
            if (path.empty() || nodes.find(path) != nodes.end()) {
                return;
            }
            EditorAssetFolder folder;
            folder.path           = path;
            const auto firstSlash = path.find('/');
            folder.mount          = firstSlash == std::string::npos ? path : path.substr(0, firstSlash);
            folder.name           = firstSlash == std::string::npos ? path : path.substr(path.rfind('/') + 1);
            folder.writable       = MountWritable(folder.mount);
            nodes.emplace(path, std::move(folder));
        };

        for (const auto &item : items) {
            const std::string dir = ParentPath(item.path);
            for (size_t i = 0; i < dir.size(); ++i) {
                if (dir[i] == '/') {
                    ensure(dir.substr(0, i));
                }
            }
            ensure(dir);
            if (dir.empty()) {
                root.items.push_back(item);
            } else {
                nodes[dir].items.push_back(item);
            }
        }

        // Phase 2: link nodes into parents, deepest first (children complete before a parent moves).
        std::vector<std::string> paths;
        paths.reserve(nodes.size());
        for (const auto &[path, node] : nodes) {
            paths.push_back(path);
        }
        std::sort(paths.begin(), paths.end(), [](const std::string &a, const std::string &b) {
            const auto depthA = std::count(a.begin(), a.end(), '/');
            const auto depthB = std::count(b.begin(), b.end(), '/');
            return depthA != depthB ? depthA > depthB : a < b;
        });
        for (const auto &path : paths) {
            const std::string parent     = ParentPath(path);
            auto              parentIter = nodes.find(parent);
            if (parent.empty() || parentIter == nodes.end()) {
                root.folders.push_back(std::move(nodes[path]));
            } else {
                parentIter->second.folders.push_back(std::move(nodes[path]));
            }
        }

        // Deterministic order.
        std::function<void(EditorAssetFolder &)> sortRecursive = [&](EditorAssetFolder &folder) {
            std::sort(folder.folders.begin(), folder.folders.end(),
                      [](const EditorAssetFolder &a, const EditorAssetFolder &b) { return a.name < b.name; });
            std::sort(folder.items.begin(), folder.items.end(),
                      [](const EditorAssetItem &a, const EditorAssetItem &b) { return a.name != b.name ? a.name < b.name : a.uuid < b.uuid; });
            for (auto &child : folder.folders) {
                sortRecursive(child);
            }
        };
        sortRecursive(root);
        return root;
    }

    EditorAssetFolder EditorAssetCatalog::ListFolder(const std::string &path)
    {
        EditorAssetFolder folder;
        folder.path = path;

        const auto items = CollectItems();
        if (path.empty()) {
            folder.name = {};
            for (const auto &item : items) {
                folder.folders.push_back(EditorAssetFolder{item.mount, item.mount, item.mount, item.writable, {}, {}});
            }
            std::sort(folder.folders.begin(), folder.folders.end(),
                      [](const EditorAssetFolder &a, const EditorAssetFolder &b) { return a.path < b.path; });
            folder.folders.erase(std::unique(folder.folders.begin(), folder.folders.end(),
                                             [](const EditorAssetFolder &a, const EditorAssetFolder &b) { return a.path == b.path; }),
                                 folder.folders.end());
            return folder;
        }

        std::string mount;
        std::string relative;
        SplitDisplay(path, mount, relative);
        folder.mount    = mount;
        folder.writable = MountWritable(mount);
        folder.name     = relative.empty() ? mount : relative.substr(relative.rfind('/') + 1);

        const std::string     prefix = path + "/";
        std::set<std::string> childFolders;
        for (const auto &item : items) {
            if (item.path.rfind(prefix, 0) != 0) {
                continue;
            }
            const std::string rest  = item.path.substr(prefix.size());
            const auto        slash = rest.find('/');
            if (slash == std::string::npos) {
                folder.items.push_back(item);
            } else {
                childFolders.insert(rest.substr(0, slash));
            }
        }

        for (const auto &child : childFolders) {
            EditorAssetFolder sub;
            sub.path     = prefix + child;
            sub.name     = child;
            sub.mount    = mount;
            sub.writable = MountWritable(mount);
            folder.folders.push_back(std::move(sub));
        }

        std::sort(folder.folders.begin(), folder.folders.end(),
                  [](const EditorAssetFolder &a, const EditorAssetFolder &b) { return a.name < b.name; });
        std::sort(folder.items.begin(), folder.items.end(), [](const EditorAssetItem &a, const EditorAssetItem &b) {
            if (a.name != b.name) {
                return a.name < b.name;
            }
            return a.uuid < b.uuid;
        });
        return folder;
    }

    bool EditorAssetCatalog::Find(const Uuid &uuid, EditorAssetItem &out) const
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return false;
        }
        auto source = db->FindAsset(uuid);
        if (!source) {
            return false;
        }
        out = MakeItem(*source);
        return true;
    }

    bool EditorAssetCatalog::FindByPath(const std::string &path, EditorAssetItem &out) const
    {
        for (const auto &item : CollectItems()) {
            if (item.path == path) {
                out = item;
                return true;
            }
        }
        return false;
    }

    AssetCookState EditorAssetCatalog::AggregateState(const CookStateMap &states)
    {
        bool anyReady   = false;
        bool anyCooking = false;
        bool anyFailed  = false;
        for (const auto &[bundle, state] : states) {
            switch (state.state) {
            case AssetCookState::Failed: anyFailed = true; break;
            case AssetCookState::Cooking: anyCooking = true; break;
            case AssetCookState::Ready: anyReady = true; break;
            default: break;
            }
        }
        if (anyFailed) {
            return AssetCookState::Failed;
        }
        if (anyCooking) {
            return AssetCookState::Cooking;
        }
        if (anyReady) {
            return AssetCookState::Ready;
        }
        return AssetCookState::NotCooked;
    }

    AssetCookState EditorAssetCatalog::GetCookState(const Uuid &uuid) const
    {
        std::lock_guard lock(stateMutex);
        if (auto iter = cookStates.find(uuid); iter != cookStates.end()) {
            return AggregateState(iter->second);
        }
        return AssetCookState::NotCooked;
    }

    AssetCookState EditorAssetCatalog::GetCookState(const Uuid &uuid, const std::string &bundle) const
    {
        std::lock_guard lock(stateMutex);
        if (auto iter = cookStates.find(uuid); iter != cookStates.end()) {
            if (auto bundleIter = iter->second.find(bundle); bundleIter != iter->second.end()) {
                return bundleIter->second.state;
            }
        }
        return AssetCookState::NotCooked;
    }

    bool EditorAssetCatalog::GetCookError(const Uuid &uuid, std::string &out) const
    {
        std::lock_guard lock(stateMutex);
        if (auto iter = cookStates.find(uuid); iter != cookStates.end()) {
            for (const auto &[bundle, state] : iter->second) {
                if (state.state == AssetCookState::Failed && !state.error.empty()) {
                    out = state.error;
                    return true;
                }
            }
        }
        return false;
    }

    bool EditorAssetCatalog::HasProduct(const Uuid &uuid, const std::string &bundle) const
    {
        auto *manager = sky::AssetManager::Get();
        return manager != nullptr && manager->HasProduct(uuid, bundle);
    }

    std::string EditorAssetCatalog::ActivePlatform() const
    {
        return sky::AssetBuilderManager::Get()->GetCookConfig().GetActivePlatform();
    }

    std::vector<std::string> EditorAssetCatalog::ResolveTargets(const sky::AssetSourceInfo &source) const
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return {};
        }
        const auto cookCfg = sky::AssetBuilderManager::Get()->GetCookConfig();
        return cookCfg.GetTargets(db->GetCookJson(source.uuid), cookCfg.GetActivePlatform());
    }

    std::vector<EditorAssetTargetInfo> EditorAssetCatalog::GetTargetInfos(const Uuid &uuid) const
    {
        std::vector<EditorAssetTargetInfo> out;
        auto                              *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return out;
        }
        auto source = db->FindAsset(uuid);
        if (!source) {
            return out;
        }

        CookStateMap states;
        {
            std::lock_guard lock(stateMutex);
            if (auto iter = cookStates.find(uuid); iter != cookStates.end()) {
                states = iter->second;
            }
        }

        for (const auto &target : ResolveTargets(*source)) {
            EditorAssetTargetInfo info;
            info.target     = target;
            info.hasProduct = HasProduct(uuid, target);
            if (auto iter = states.find(target); iter != states.end()) {
                info.state = iter->second.state;
                info.error = iter->second.error;
            }
            const auto &cookCfg  = sky::AssetBuilderManager::Get()->GetCookConfig();
            const auto  override = cookCfg.GetTargetSettings(db->GetCookJson(uuid), target);
            const auto  bundle   = cookCfg.ResolveBundleForTarget(target);
            for (const auto &[key, value] : sky::AssetBuilderManager::Get()->GetBuilderSettings(source->ext, bundle, override)) {
                info.settings.push_back({key, value});
            }
            out.push_back(std::move(info));
        }
        return out;
    }

    std::vector<Uuid> EditorAssetCatalog::GetDependencies(const Uuid &uuid) const
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return {};
        }
        auto source = db->FindAsset(uuid);
        return source != nullptr ? source->dependencies : std::vector<Uuid>{};
    }

    std::vector<Uuid> EditorAssetCatalog::GetDependents(const Uuid &uuid) const
    {
        // Build the reverse graph on demand (not just on an explicit Refresh).
        if (!graphBuilt) {
            RebuildGraph();
        }
        std::lock_guard lock(stateMutex);
        if (auto iter = dependents.find(uuid); iter != dependents.end()) {
            return iter->second;
        }
        return {};
    }

    EditorAssetCookConfig EditorAssetCatalog::GetCookConfig(const Uuid &uuid) const
    {
        EditorAssetCookConfig config;
        auto                 *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return config;
        }

        const auto cookCfg    = sky::AssetBuilderManager::Get()->GetCookConfig();
        config.activePlatform = cookCfg.GetActivePlatform();

        std::string active;
        if (db->GetTarget(uuid, active)) {
            config.activeTarget = active;
        }

        const auto cookJson = db->GetCookJson(uuid);
        config.targets      = cookCfg.GetTargets(cookJson, config.activePlatform);
        if (config.activeTarget.empty() && !config.targets.empty()) {
            config.activeTarget = config.targets.front();
        }
        return config;
    }

    bool EditorAssetCatalog::IsWritable(const std::string &path) const
    {
        EditorAssetItem item;
        return FindByPath(path, item) && item.writable;
    }

    void EditorAssetCatalog::BeginCook(const Uuid &uuid, const std::vector<std::string> &targets)
    {
        {
            std::lock_guard lock(stateMutex);
            auto           &states = cookStates[uuid];
            for (const auto &target : targets) {
                states[target].state = AssetCookState::Cooking;
                states[target].error.clear();
            }
        }
        BumpRevision();
        TrackCook(uuid);
    }

    EditorCookSettings EditorAssetCatalog::GetCookSettings(const Uuid &uuid, const std::string &target) const
    {
        EditorCookSettings out;
        auto              *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return out;
        }
        auto source = db->FindAsset(uuid);
        if (!source || source->ext.empty()) {
            return out;
        }

        auto *builders = sky::AssetBuilderManager::Get();
        if (builders->GetBuilderSettingsType(source->ext) == nullptr) {
            return out;
        }
        const auto &cookCfg  = builders->GetCookConfig();
        const auto  override = cookCfg.GetTargetSettings(db->GetCookJson(uuid), target);
        const auto  bundle   = cookCfg.ResolveBundleForTarget(target);

        out.object   = builders->MakeBuilderSettings(source->ext, bundle, override);
        out.baseline = builders->MakeBuilderSettings(source->ext, bundle, {});
        out.type     = sky::GetTypeNode(out.object);
        return out;
    }

    bool EditorAssetCatalog::ApplyCookSettings(const Uuid &uuid, const std::string &target, const Any &edited)
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return false;
        }
        auto source = db->FindAsset(uuid);
        if (!source || source->ext.empty()) {
            return false;
        }

        const auto &cookCfg  = sky::AssetBuilderManager::Get()->GetCookConfig();
        const auto  bundle   = cookCfg.ResolveBundleForTarget(target);
        const auto  override = sky::AssetBuilderManager::Get()->DiffBuilderSettings(source->ext, bundle, edited);

        rapidjson::Document cook;
        ParseCook(db->GetCookJson(uuid), cook);
        auto &allocator = cook.GetAllocator();

        if (!cook.HasMember("settings") || !cook["settings"].IsObject()) {
            cook.RemoveMember("settings");
            cook.AddMember("settings", rapidjson::Value(rapidjson::kObjectType), allocator);
        }
        rapidjson::Value &settings = cook["settings"];
        settings.RemoveMember(target.c_str());

        if (!override.empty()) {
            rapidjson::Value targetObj(rapidjson::kObjectType);
            for (const auto &[key, value] : override) {
                targetObj.AddMember(rapidjson::StringRef(key.c_str()), rapidjson::Value(value.c_str(), allocator), allocator);
            }
            settings.AddMember(rapidjson::StringRef(target.c_str()), targetObj, allocator);
        }
        if (settings.MemberCount() == 0) {
            cook.RemoveMember("settings");
        }

        if (!db->SetCookJson(uuid, cook.MemberCount() == 0 ? std::string{} : SerializeJson(cook))) {
            return false;
        }
        BumpRevision();
        NotifyObservers();
        return true;
    }

    void EditorAssetCatalog::TrackCook(const Uuid &uuid)
    {
        if (std::find(trackedCooks.begin(), trackedCooks.end(), uuid) != trackedCooks.end()) {
            return;
        }
        trackedCooks.push_back(uuid);
        sky::AsseEvent::Connect(uuid, listener.get());
    }

    void EditorAssetCatalog::RebuildGraph() const
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return;
        }
        sky::AssetDependencyGraph graph;
        db->BuildDependencyGraph(graph);

        std::unordered_map<Uuid, std::vector<Uuid>> reverse;
        graph.ForEach([&](const Uuid &id, const std::vector<Uuid> &deps) {
            for (const auto &dep : deps) {
                reverse[dep].push_back(id);
            }
        });

        std::lock_guard lock(stateMutex);
        dependents = std::move(reverse);
        graphBuilt = true;
    }

    void EditorAssetCatalog::Refresh()
    {
        RebuildGraph();
        BumpRevision();
        NotifyObservers();
    }

    uint32_t EditorAssetCatalog::AddObserver(Observer observer)
    {
        std::lock_guard lock(observerMutex);
        const uint32_t  id = nextObserverId++;
        observers.emplace(id, std::move(observer));
        return id;
    }

    void EditorAssetCatalog::RemoveObserver(uint32_t id)
    {
        std::lock_guard lock(observerMutex);
        observers.erase(id);
    }

    void EditorAssetCatalog::NotifyObservers()
    {
        std::vector<Observer> snapshot;
        {
            std::lock_guard lock(observerMutex);
            snapshot.reserve(observers.size());
            for (auto &[id, observer] : observers) {
                snapshot.push_back(observer);
            }
        }
        for (auto &observer : snapshot) {
            if (observer) {
                observer();
            }
        }
    }

    void EditorAssetCatalog::OnBuildFinished(const Uuid &uuid, const std::string &bundle, bool success, const std::string &error)
    {
        {
            std::lock_guard lock(stateMutex);
            BuildState     &state = cookStates[uuid][bundle];
            state.state           = success ? AssetCookState::Ready : AssetCookState::Failed;
            state.error           = error;
        }
        // Bumped here (may run on a cook thread); views poll GetStateRevision() and re-read on the
        // next paint. Observers are intentionally NOT notified here: this can run off the main thread.
        BumpRevision();
    }

    namespace {
        IEditorAssetCatalog *g_catalogOverride = nullptr;
    } // namespace

    IEditorAssetCatalog *GetEditorAssetCatalog()
    {
        if (g_catalogOverride != nullptr) {
            return g_catalogOverride;
        }
        return EditorAssetCatalog::Get();
    }

    void SetEditorAssetCatalog(IEditorAssetCatalog *catalog)
    {
        g_catalogOverride = catalog;
    }

} // namespace sky::editor
