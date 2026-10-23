//
// Created on 2026/10/23.
//

#include "ViewportPickIndex.h"
#include "ViewportPickMath.h"
#include <editor/render/CpuViewportPicker.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <core/tree/Bvh.h>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <thread>
#include <tuple>

namespace sky::editor {
    struct PickTriangle {
        Vector3  a, b, c;
        uint32_t part = 0;
    };
} // namespace sky::editor
namespace sky {
    template <>
    struct BvhTraits<editor::PickTriangle> {
        static AABB GetBounds(const editor::PickTriangle &t)
        {
            return {Min(t.a, Min(t.b, t.c)), Max(t.a, Max(t.b, t.c))};
        }
    };
} // namespace sky
namespace sky::editor {
    namespace {
        using namespace aurora;
        using Clock = std::chrono::steady_clock;
        using Key   = std::tuple<Uuid, uint64_t, uint64_t, uint64_t, uint64_t>;
        auto TargetKey(const ViewportPickTarget &target)
        {
            return std::tuple{target.actor, target.component, target.instance, target.generation, target.provider, target.part, target.subelement};
        }
        auto OverlayKey(int32_t priority, const ViewportPickTarget &target)
        {
            return std::tuple{-int64_t(priority), TargetKey(target)};
        }
        double Milliseconds(Clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        }
        using namespace pick;
        bool Intersect(const PickTriangle &triangle, Vector3 origin, Vector3 direction, float &distance)
        {
            // Double precision keeps small triangles and non-uniform transforms usable without a world-scale epsilon.
            const auto   edge1 = triangle.b - triangle.a, edge2 = triangle.c - triangle.a, offset = origin - triangle.a;
            const double px          = double(direction.y) * edge2.z - double(direction.z) * edge2.y;
            const double py          = double(direction.z) * edge2.x - double(direction.x) * edge2.z;
            const double pz          = double(direction.x) * edge2.y - double(direction.y) * edge2.x;
            const double determinant = edge1.x * px + edge1.y * py + edge1.z * pz;
            if (determinant == 0 || !std::isfinite(determinant))
                return false;
            const double u  = (offset.x * px + offset.y * py + offset.z * pz) / determinant;
            const double qx = double(offset.y) * edge1.z - double(offset.z) * edge1.y;
            const double qy = double(offset.z) * edge1.x - double(offset.x) * edge1.z;
            const double qz = double(offset.x) * edge1.y - double(offset.y) * edge1.x;
            const double v  = (direction.x * qx + direction.y * qy + direction.z * qz) / determinant;
            const double t  = (edge2.x * qx + edge2.y * qy + edge2.z * qz) / determinant;
            if (u < 0 || v < 0 || u + v > 1 || !std::isfinite(t))
                return false;
            distance = static_cast<float>(t);
            return true;
        }
        Key GeometryKey(const RenderResourcePayload &payload)
        {
            uint64_t   layout = 14695981039346656037ull;
            const auto mix    = [&](uint64_t value) { layout = (layout ^ value) * 1099511628211ull; };
            if (payload.mesh) {
                for (const auto &stream : payload.mesh->streams) {
                    mix(stream.offset);
                    mix(stream.size);
                    mix(stream.stride);
                    mix(uint64_t(stream.rate));
                }
                for (const auto &attribute : payload.mesh->attributes) {
                    mix(attribute.binding);
                    mix(attribute.offset);
                    mix(uint64_t(attribute.format));
                    mix(attribute.semanticIndex);
                    for (const auto c : attribute.semantic)
                        mix(uint8_t(c));
                }
                if (!payload.mesh->lods.empty()) {
                    const auto &lod = payload.mesh->lods.front();
                    mix(lod.offset);
                    mix(lod.size);
                    mix(uint64_t(lod.type));
                    const auto draw = [&](const CmdDrawIndexed &command) {
                        mix(command.firstIndex);
                        mix(command.indexCount);
                        mix(uint64_t(command.vertexOffset));
                        mix(command.instanceCount);
                    };
                    draw(lod.command);
                    for (const auto &section : lod.sections)
                        draw(section);
                }
            }
            const auto &key = payload.resource;
            return {key.product, key.revision, key.usageVariant, key.deviceEpoch, layout};
        }
        std::shared_ptr<const RenderResourcePayload> Geometry(const SceneGeometryInstance &instance, const SceneResourcePart &part)
        {
            for (const auto &payload : instance.resources->payloads)
                if (payload.resource == part.mesh.resource)
                    return {instance.resources, &payload};
            return {};
        }
        uint64_t TriangleCount(const RenderResourcePayload &payload)
        {
            if (!payload.mesh || payload.mesh->lods.empty())
                return 0;
            const auto &lod = payload.mesh->lods.front();
            if (lod.sections.empty())
                return lod.command.indexCount / 3;
            uint64_t count = 0;
            for (const auto &section : lod.sections)
                count += section.indexCount / 3;
            return count;
        }
        struct GeometryData {
            Bvh<PickTriangle>  bvh;
            ViewportPickStatus status = ViewportPickStatus::ERROR;
            std::string        message;
            uint64_t           degenerate         = 0;
            double             decodeMilliseconds = 0;
        };
        void Build(const RenderResourcePayload &payload, GeometryData &out, const std::atomic<bool> &cancelled)
        {
            const auto decodeStart = Clock::now();
            out.message            = "Invalid static triangle mesh layout or range";
            if (!payload.mesh || payload.mesh->lods.empty())
                return;
            const auto &mesh  = *payload.mesh;
            const auto  found = std::find_if(mesh.attributes.begin(), mesh.attributes.end(),
                                             [](const auto &a) { return a.semantic == "POSITION" && a.semanticIndex == 0; });
            if (found == mesh.attributes.end() || found->format != Format::F_RGB32 || found->binding >= mesh.streams.size())
                return;
            const auto &stream = mesh.streams[found->binding];
            if (stream.rate != VertexInputRate::PER_VERTEX || stream.stride < 12 || found->offset > stream.stride - 12 ||
                stream.offset > payload.bytes.size() || stream.size > payload.bytes.size() - stream.offset || stream.size % stream.stride)
                return;
            const auto &lod = mesh.lods.front();
            if ((lod.type != IndexType::U16 && lod.type != IndexType::U32) || lod.offset > payload.bytes.size() ||
                lod.size > payload.bytes.size() - lod.offset)
                return;
            const uint64_t indexSize = lod.type == IndexType::U16 ? 2 : 4;
            if (lod.size % indexSize)
                return;
            const auto count = TriangleCount(payload);
            if (count > UINT32_MAX)
                return;
            std::vector<PickTriangle> triangles;
            triangles.reserve(static_cast<size_t>(count));
            const size_t parts = std::max(size_t{1}, lod.sections.size());
            for (size_t part = 0; part < parts; ++part) {
                const auto &command = lod.sections.empty() ? lod.command : lod.sections[part];
                if (command.indexCount % 3 || command.instanceCount != 1 || command.firstInstance != 0 || command.firstIndex > lod.size / indexSize ||
                    command.indexCount > lod.size / indexSize - command.firstIndex)
                    return;
                for (uint32_t i = 0; i < command.indexCount; i += 3) {
                    if (cancelled.load(std::memory_order_relaxed))
                        return;
                    PickTriangle triangle;
                    Vector3     *points[] = {&triangle.a, &triangle.b, &triangle.c};
                    for (uint32_t vertex = 0; vertex < 3; ++vertex) {
                        uint32_t index = 0;
                        std::memcpy(&index, payload.bytes.data() + lod.offset + (uint64_t(command.firstIndex) + i + vertex) * indexSize, indexSize);
                        const int64_t adjusted = int64_t(index) + command.vertexOffset;
                        if (adjusted < 0 || uint64_t(adjusted) >= stream.size / stream.stride)
                            return;
                        std::memcpy(points[vertex], payload.bytes.data() + stream.offset + uint64_t(adjusted) * stream.stride + found->offset, 12);
                        if (!Finite(*points[vertex])) {
                            out.message = "Mesh contains non-finite positions";
                            return;
                        }
                    }
                    const auto normal = (triangle.b - triangle.a).Cross(triangle.c - triangle.a);
                    if (normal.x == 0 && normal.y == 0 && normal.z == 0) {
                        ++out.degenerate;
                        continue;
                    }
                    triangle.part = static_cast<uint32_t>(part);
                    triangles.push_back(triangle);
                }
            }
            if (cancelled.load(std::memory_order_relaxed))
                return;
            out.decodeMilliseconds = Milliseconds(decodeStart);
            out.bvh.Build(triangles, &cancelled);
            if (cancelled.load(std::memory_order_relaxed))
                return;
            out.status  = ViewportPickStatus::HIT;
            out.message = out.degenerate ? "Degenerate triangles skipped" : "";
        }
    } // namespace

    struct CpuViewportPicker::Impl {
        struct ProviderEntry {
            std::shared_ptr<const void>                      moduleLease;
            std::shared_ptr<IViewportPickProvider>           provider;
            std::shared_ptr<const ViewportPickProxySnapshot> snapshot;
            uint64_t                                         generation = 0;
        };
        struct Providers {
            std::map<uint64_t, ProviderEntry> entries;
            uint64_t                          generation = 1, next = 0;
        };
        struct ProviderScope {
            std::weak_ptr<Providers> owner;
            uint64_t                 id = 0, generation = 0;
            ~ProviderScope()
            {
                if (const auto state = owner.lock()) {
                    const auto entry = state->entries.find(id);
                    if (entry != state->entries.end() && entry->second.generation == generation) {
                        state->entries.erase(entry);
                        ++state->generation;
                    }
                }
            }
        };
        std::shared_ptr<Providers>     providers = std::make_shared<Providers>();
        std::vector<ViewportPickProxy> proxies;
        struct CustomGeometry {
            std::shared_ptr<const void> moduleLease;
            ViewportPickGeometry        source;
        };
        std::vector<CustomGeometry> customGeometries;
        uint64_t                    preparedProviders = 0, proxyDataGeneration = 1;
        ViewportPickStatus          proxyStatus = ViewportPickStatus::HIT;
        std::string                 proxyError;
        ViewportPickReason          proxyReason = ViewportPickReason::NONE;
        void                        PrepareProviders()
        {
            bool         changed  = preparedProviders != providers->generation || proxyStatus == ViewportPickStatus::ERROR;
            const size_t capacity = 4096;
            const auto slots = [](const ViewportPickProxySnapshot &snapshot) { return snapshot.proxies.capacity() + snapshot.geometries.capacity(); };
            size_t     retained = 0;
            for (const auto &[id, entry] : providers->entries)
                if (entry.snapshot)
                    retained += slots(*entry.snapshot);
            for (auto &[id, entry] : providers->entries) {
                const auto snapshot = entry.provider->Snapshot();
                if (snapshot && snapshot == entry.snapshot)
                    continue;
                if (!snapshot || snapshot->message.size() > 4096 ||
                    slots(*snapshot) > capacity - (retained - (entry.snapshot ? slots(*entry.snapshot) : 0)) || !snapshot->revision ||
                    (entry.snapshot && snapshot->revision < entry.snapshot->revision)) {
                    proxyStatus = ViewportPickStatus::ERROR;
                    proxyReason = snapshot && slots(*snapshot) > capacity - (retained - (entry.snapshot ? slots(*entry.snapshot) : 0))
                                      ? ViewportPickReason::RESOURCE_LIMIT
                                      : ViewportPickReason::INVALID_REQUEST;
                    proxyError  = proxyReason == ViewportPickReason::RESOURCE_LIMIT ? "Provider proxy memory budget exceeded"
                                                                                    : "Invalid or regressing provider revision";
                    ++providers->generation;
                    preparedProviders = providers->generation;
                    return;
                }
                if (!entry.snapshot || entry.snapshot->revision != snapshot->revision)
                    ++providers->generation;
                retained -= entry.snapshot ? slots(*entry.snapshot) : 0;
                retained += slots(*snapshot);
                entry.snapshot = snapshot;
                changed        = true;
            }
            if (!changed)
                return;
            ++indexGeneration;
            ++proxyDataGeneration;
            proxyStatus = ViewportPickStatus::HIT;
            proxyError.clear();
            proxyReason = ViewportPickReason::NONE;
            proxies.clear();
            customGeometries.clear();
            for (const auto &[id, entry] : providers->entries) {
                const auto &snapshot = *entry.snapshot;
                if (snapshot.status != ViewportPickStatus::HIT && snapshot.status != ViewportPickStatus::MISS) {
                    proxyStatus = snapshot.status;
                    proxyError  = snapshot.message;
                    proxyReason =
                        snapshot.status == ViewportPickStatus::UNSUPPORTED ? ViewportPickReason::UNSUPPORTED_COVERAGE : ViewportPickReason::NONE;
                    break;
                }
                if (snapshot.proxies.size() > capacity - proxies.size()) {
                    proxyStatus = ViewportPickStatus::ERROR;
                    proxyError  = "Provider proxy memory budget exceeded";
                    proxyReason = ViewportPickReason::RESOURCE_LIMIT;
                    break;
                }
                std::set<std::tuple<Uuid, Uuid, uint64_t, uint64_t, uint32_t, uint32_t>> identities;
                for (auto proxy : snapshot.proxies) {
                    const auto &target = proxy.target;
                    bool        valid =
                        target.actor != Uuid{} && target.generation && (!target.provider || target.provider == id) &&
                        identities.emplace(target.actor, target.component, target.instance, target.generation, target.part, target.subelement).second;
                    if (const auto *triangle = std::get_if<ViewportTriangleProxy>(&proxy.shape))
                        valid &= Finite(triangle->a) && Finite(triangle->b) && Finite(triangle->c);
                    else if (const auto *line = std::get_if<ViewportSegmentProxy>(&proxy.shape))
                        valid &= Finite(line->a) && Finite(line->b) && std::isfinite(line->width) && line->width > 0 &&
                                 std::isfinite(line->tolerance) && line->tolerance >= 0;
                    else {
                        const auto &icon = std::get<ViewportBillboardProxy>(proxy.shape);
                        valid &=
                            Finite(icon.center) && std::isfinite(icon.size.x) && std::isfinite(icon.size.y) && icon.size.x > 0 && icon.size.y > 0;
                    }
                    if (!valid) {
                        proxyStatus = ViewportPickStatus::ERROR;
                        proxyError  = "Invalid or duplicate provider proxy identity/shape";
                        proxyReason = ViewportPickReason::INVALID_REQUEST;
                        break;
                    }
                    proxy.target.provider = id;
                    proxies.push_back(std::move(proxy));
                }
                if (proxyStatus != ViewportPickStatus::HIT)
                    break;
                for (auto source : snapshot.geometries) {
                    const auto &target = source.target;
                    if (target.actor == Uuid{} || !target.generation || (target.provider && target.provider != id) || !source.geometry ||
                        (source.bounds && !ValidBounds(*source.bounds)) ||
                        !identities.emplace(target.actor, target.component, target.instance, target.generation, target.part, target.subelement)
                             .second) {
                        proxyStatus = ViewportPickStatus::ERROR;
                        proxyError  = "Invalid or duplicate custom geometry identity/bounds/query";
                        proxyReason = ViewportPickReason::INVALID_REQUEST;
                        break;
                    }
                    source.target.provider = id;
                    customGeometries.push_back({entry.moduleLease, std::move(source)});
                }
                if (proxyStatus != ViewportPickStatus::HIT)
                    break;
            }
            preparedProviders = providers->generation;
        }
        struct Projection {
            ViewportPickView                 view;
            ViewportPickIndex                index{1024 * 1024};
            std::vector<ViewportProxyLayout> layouts;
            size_t                           next      = 0;
            uint64_t                         providers = 0, serial = 0, used = 0;
            bool                             failed = false;
        };
        std::vector<std::unique_ptr<Projection>> projections;
        uint64_t                                 projectionSerial = 0, projectionUse = 0;
        Projection                              &ProjectProxies(const EditorCamera &camera, Vector2 extent)
        {
            ViewportPickRequest key;
            key.view.camera    = camera;
            key.view.extent    = extent;
            Projection *chosen = nullptr;
            for (const auto &entry : projections) {
                ViewportPickRequest existing;
                existing.view = entry->view;
                if (SameViewportPickRequest(key, existing) && entry->providers == proxyDataGeneration) {
                    chosen = entry.get();
                    break;
                }
            }
            if (!chosen) {
                if (projections.size() == 8) {
                    const auto oldest =
                        std::min_element(projections.begin(), projections.end(), [](const auto &a, const auto &b) { return a->used < b->used; });
                    projections.erase(oldest);
                }
                auto entry       = std::make_unique<Projection>();
                entry->view      = key.view;
                entry->providers = proxyDataGeneration;
                entry->serial    = ++projectionSerial;
                entry->layouts.resize(proxies.size());
                chosen = entry.get();
                projections.push_back(std::move(entry));
            }
            chosen->used     = ++projectionUse;
            const auto start = Clock::now();
            while (!chosen->failed && chosen->next < proxies.size()) {
                const auto  at     = chosen->next++;
                const auto &proxy  = proxies[at];
                auto       &layout = chosen->layouts[at];
                ++stats.projectionUpdates;
                if (ProjectViewportPickProxy(proxy, camera, extent, layout)) {
                    AABB bounds{{INFINITY, INFINITY, 0}, {-INFINITY, -INFINITY, 0}};
                    for (uint32_t i = 0; i < layout.count; ++i) {
                        bounds.min = Min(bounds.min, {layout.points[i].x, layout.points[i].y, 0});
                        bounds.max = Max(bounds.max, {layout.points[i].x, layout.points[i].y, 0});
                    }
                    if (const auto *line = std::get_if<ViewportSegmentProxy>(&proxy.shape)) {
                        bounds.min.x -= line->tolerance;
                        bounds.min.y -= line->tolerance;
                        bounds.max.x += line->tolerance;
                        bounds.max.y += line->tolerance;
                    }
                    if (chosen->index.Insert(uint32_t(at), bounds) == ViewportPickIndex::NONE)
                        chosen->failed = true;
                }
                if (chosen->next % 32 == 0 && Milliseconds(start) >= 1.0)
                    break;
            }
            return *chosen;
        }
        struct Entry {
            std::shared_ptr<GeometryData> data;
            uint64_t                      used = 0, request = 0;
        };
        struct Job {
            Key                                                  key;
            uint64_t                                             document = 0, scene = 0, request = 0;
            std::shared_ptr<const aurora::RenderResourcePayload> payload;
            std::shared_ptr<GeometryData>                        data = std::make_shared<GeometryData>();
            std::atomic<bool>                                    done{false}, cancelled{false};
            double                                               milliseconds = 0;
            std::thread                                          thread;
            ~Job()
            {
                cancelled.store(true);
                if (thread.joinable())
                    thread.join();
            }
        };
        struct Part {
            std::shared_ptr<const aurora::RenderResourcePayload> payload;
            Key                                                  key;
        };
        struct Instance {
            aurora::SceneGeometryInstance source;
            Matrix4                       inverse;
            std::vector<Part>             parts;
            uint32_t                      leaf     = ViewportPickIndex::NONE;
            uint64_t                      seen     = 0;
            bool                          occupied = false, valid = false;
        };
        std::deque<Instance>                                 instances;
        std::vector<uint32_t>                                freeInstances;
        std::map<Uuid, uint32_t>                             instanceMap;
        std::map<Key, uint32_t>                              geometryUsers;
        std::set<uint32_t>                                   unknown;
        std::vector<uint32_t>                                unknownList;
        ViewportPickIndex                                    index{8ull * 1024 * 1024};
        std::shared_ptr<const aurora::SceneGeometrySnapshot> syncing;
        uint64_t                                             appliedVersion = 0, indexGeneration = 1, syncEpoch = 0;
        size_t                                               syncCursor = 0, removeCursor = 0;
        bool                                                 fullSync = false;
        std::string                                          indexError;
        ViewportPickReason                                   indexReason = ViewportPickReason::INVALID_REQUEST;
        struct Traversal {
            struct CustomQuery {
                // Foreign cursor and geometry destruct before their module lease.
                std::shared_ptr<const void>                  moduleLease;
                std::shared_ptr<const IViewportPickGeometry> geometry;
                std::unique_ptr<IViewportPickGeometryCursor> cursor;
            };
            std::unique_ptr<CustomQuery>  customQuery;
            size_t                        customAt = 0;
            ViewportPickRequest           request;
            ViewportPickResult            result;
            ViewportPickIndex::Cursor     cursor;
            Bvh<PickTriangle>::RayCursor  meshCursor;
            std::shared_ptr<GeometryData> geometry;
            uint64_t                      generation = 0, projection = 0;
            ViewportPickIndex::Cursor     proxyCursor;
            bool                          proxyDone = false;
            uint32_t                      current   = ViewportPickIndex::NONE;
            size_t                        unknownAt = 0, part = 0, proxyAt = 0;
            int32_t                       priority       = 0;
            float                         screenDistance = 0;
            bool                          overlay        = false;
            float                         entry = 0, nearest = 0, unresolvedDistance = 0;
            ViewportPickStatus            unresolved       = ViewportPickStatus::MISS;
            ViewportPickReason            unresolvedReason = ViewportPickReason::NONE;
            std::string                   diagnostic;
            bool                          initialized = false, done = false, tiePass = false, surface = false;
        };
        Traversal  direct;
        Traversal *activeTraversal = nullptr;
        struct RequestSlot {
            ViewportPickRequest               request;
            std::optional<ViewportPickResult> result;
            Traversal                         traversal;
        };
        std::map<std::pair<uint64_t, uint64_t>, RequestSlot> requests;
        double                                               queryAllowance = 1.0;
        std::pair<uint64_t, uint64_t>                        nextView{};
        uint64_t                                             budget, serial = 0, use = 0;
        size_t                                               entryBudget;
        std::map<Key, Entry>                                 cache;
        std::unique_ptr<Job>                                 job;
        std::shared_ptr<const aurora::RenderResourcePayload> wanted;
        uint64_t                                             wantedRequest = 0, rejectedRequest = 0;
        std::optional<Key>                                   rejected;
        std::shared_ptr<const aurora::SceneGeometrySnapshot> scene;
        ViewportPickContext                                  context;
        ViewportPickStatistics                               stats;

        bool Present(const Key &key) const
        {
            return geometryUsers.contains(key);
        }
        void ReleaseParts(Instance &instance)
        {
            for (const auto &part : instance.parts) {
                if (!part.payload)
                    continue;
                const auto found = geometryUsers.find(part.key);
                if (found != geometryUsers.end() && --found->second == 0)
                    geometryUsers.erase(found);
            }
            instancePartBytes -= instance.parts.capacity() * sizeof(Part);
            std::vector<Part>().swap(instance.parts);
        }
        void RemoveInstance(uint32_t id)
        {
            auto &instance = instances[id];
            ReleaseParts(instance);
            if (instance.leaf != ViewportPickIndex::NONE)
                index.Remove(instance.leaf);
            unknown.erase(id);
            instanceMap.erase(instance.source.actor);
            instanceMessageBytes -= instance.source.message.capacity();
            instance = {};
            freeInstances.push_back(id);
            ++stats.indexUpdates;
        }
        uint64_t IndexBytes() const
        {
            uint64_t bytes = index.Bytes() + instances.size() * (sizeof(Instance) + 32) + instanceMap.size() * 96 + geometryUsers.size() * 128 +
                             unknown.size() * 64 + unknownList.capacity() * sizeof(uint32_t) + freeInstances.capacity() * sizeof(uint32_t);
            return bytes + instancePartBytes + instanceMessageBytes;
        }
        uint64_t instancePartBytes = 0, instanceMessageBytes = 0;
        void     UpdateInstance(const aurora::SceneGeometryInstance &source)
        {
            const auto partCount = source.resources ? source.resources->parts.size() : 0;
            if (partCount > 256 || (source.resources && source.resources->payloads.size() > 512) || source.message.size() > 4096 ||
                IndexBytes() + 2 * index.Bytes() + sizeof(Instance) + 512 + partCount * (sizeof(Part) + 128) + source.message.size() >
                    32ull * 1024 * 1024) {
                indexError  = "Picking instance index exceeds its memory budget";
                indexReason = ViewportPickReason::RESOURCE_LIMIT;
                return;
            }
            auto found = instanceMap.find(source.actor);
            if (found == instanceMap.end()) {
                uint32_t id = 0;
                if (freeInstances.empty()) {
                    if (instances.size() >= 100000) {
                        indexError  = "Picking instance index exceeds its memory budget";
                        indexReason = ViewportPickReason::RESOURCE_LIMIT;
                        return;
                    }
                    id = uint32_t(instances.size());
                    instances.emplace_back();
                } else {
                    id = freeInstances.back();
                    freeInstances.pop_back();
                }
                found = instanceMap.emplace(source.actor, id).first;
            }
            const auto id       = found->second;
            auto      &instance = instances[id];
            if (instance.occupied && instance.seen == syncEpoch) {
                indexError = "Duplicate instance identity in picking scene delta";
                return;
            }
            instance.seen = syncEpoch;
            const bool transform =
                !instance.occupied || !std::equal(std::begin(instance.source.world.v), std::end(instance.source.world.v), std::begin(source.world.v));
            const bool data    = !instance.occupied || instance.source.resources != source.resources;
            const bool changed = transform || data || instance.source.state != source.state || instance.source.generation != source.generation ||
                                 instance.source.visibilityMask != source.visibilityMask ||
                                 instance.source.bounds.has_value() != source.bounds.has_value() ||
                                 (source.bounds && instance.source.bounds &&
                                  (source.bounds->min != instance.source.bounds->min || source.bounds->max != instance.source.bounds->max));
            if (transform) {
                const float determinant = source.world.Determinant();
                instance.valid = std::all_of(std::begin(source.world.v), std::end(source.world.v), [](float v) { return std::isfinite(v); }) &&
                                 std::isfinite(determinant) && determinant != 0 && source.world.v[3] == 0 && source.world.v[7] == 0 &&
                                 source.world.v[11] == 0 && source.world.v[15] == 1;
                if (instance.valid)
                    instance.inverse = source.world.Inverse();
                ++stats.inverseUpdates;
            }
            if (data) {
                ReleaseParts(instance);
                instance.parts.reserve(partCount);
                instancePartBytes += instance.parts.capacity() * sizeof(Part);
                if (source.resources)
                    for (const auto &part : source.resources->parts) {
                        auto payload = Geometry(source, part);
                        auto key     = payload ? GeometryKey(*payload) : Key{};
                        if (payload)
                            ++geometryUsers[key];
                        instance.parts.push_back({std::move(payload), key});
                    }
            }
            if (instance.occupied)
                instanceMessageBytes -= instance.source.message.capacity();
            instance.source = source;
            instanceMessageBytes += instance.source.message.capacity();
            instance.occupied = true;
            if (!changed)
                return;
            ++stats.indexUpdates;
            std::optional<AABB> local = source.bounds;
            if (!local && !instance.parts.empty()) {
                for (const auto &part : instance.parts) {
                    if (!part.payload || !part.payload->metadata.bounds || !ValidBounds(*part.payload->metadata.bounds)) {
                        local.reset();
                        break;
                    }
                    local = local ? Merge(*local, *part.payload->metadata.bounds) : *part.payload->metadata.bounds;
                }
            }
            std::optional<AABB> world;
            if (local && ValidBounds(*local) && source.world.v[3] == 0 && source.world.v[7] == 0 && source.world.v[11] == 0 &&
                source.world.v[15] == 1) {
                AABB transformed{Vector3{INFINITY}, Vector3{-INFINITY}};
                for (uint32_t corner = 0; corner < 8; ++corner) {
                    const auto point = TransformPoint(source.world,
                                                      {corner & 1 ? local->max.x : local->min.x, corner & 2 ? local->max.y : local->min.y,
                                                       corner & 4 ? local->max.z : local->min.z},
                                                      1);
                    if (!Finite(point)) {
                        transformed.min = Vector3{NAN};
                        break;
                    }
                    transformed.min = Min(transformed.min, point);
                    transformed.max = Max(transformed.max, point);
                }
                if (ValidBounds(transformed))
                    world = transformed;
            }
            unknown.erase(id);
            if (world && source.visibilityMask) {
                if (instance.leaf == ViewportPickIndex::NONE)
                    instance.leaf = index.Insert(id, *world);
                else
                    index.Update(instance.leaf, *world);
                if (instance.leaf == ViewportPickIndex::NONE) {
                    indexError  = "Picking instance tree exceeds its memory budget";
                    indexReason = ViewportPickReason::RESOURCE_LIMIT;
                }
            } else {
                if (instance.leaf != ViewportPickIndex::NONE)
                    index.Remove(instance.leaf);
                instance.leaf = ViewportPickIndex::NONE;
                if (source.visibilityMask)
                    unknown.insert(id);
            }
        }
        void SyncIndex(const std::shared_ptr<const aurora::SceneGeometrySnapshot> &next)
        {
            const auto start = Clock::now();
            if (!next)
                return;
            if (next->version && (next->version < appliedVersion || (syncing && next->version < syncing->version))) {
                indexError  = "Picking scene snapshot revision regressed";
                indexReason = ViewportPickReason::STALE_REQUEST;
                syncing.reset();
                ++indexGeneration;
                return;
            }
            if (!syncing && next->version && next->version == appliedVersion)
                return;
            if (!syncing || (next->version && next->version != syncing->version)) {
                fullSync   = !next->version || syncing || next->baseVersion != appliedVersion || !appliedVersion;
                syncing    = next;
                syncCursor = removeCursor = 0;
                ++syncEpoch;
                ++indexGeneration;
                indexError.clear();
                indexReason = ViewportPickReason::INVALID_REQUEST;
            }
            const auto count = fullSync ? syncing->instances.size() : syncing->changed.size();
            uint32_t   batch = 0;
            while (syncCursor < count) {
                const auto at = fullSync ? syncCursor : syncing->changed[syncCursor];
                if (at >= syncing->instances.size()) {
                    indexError = "Invalid picking scene delta";
                    break;
                }
                UpdateInstance(syncing->instances[at]);
                ++syncCursor;
                if (!indexError.empty())
                    break;
                if (++batch % 32 == 0 && Milliseconds(start) >= 1.0)
                    return;
            }
            if (indexError.empty()) {
                const auto countRemove = fullSync ? instances.size() : syncing->removed.size();
                while (removeCursor < countRemove) {
                    if (fullSync) {
                        if (instances[removeCursor].occupied && instances[removeCursor].seen != syncEpoch)
                            RemoveInstance(uint32_t(removeCursor));
                    } else {
                        const auto found = instanceMap.find(syncing->removed[removeCursor]);
                        if (found != instanceMap.end())
                            RemoveInstance(found->second);
                    }
                    ++removeCursor;
                    if (++batch % 32 == 0 && Milliseconds(start) >= 1.0)
                        return;
                }
            }
            unknownList.assign(unknown.begin(), unknown.end());
            appliedVersion = syncing->version;
            syncing.reset();
            stats.indexBytes              = IndexBytes();
            stats.maintenanceMilliseconds = Milliseconds(start);
        }
        void Prepare()
        {
            if (job && (job->document != context.document || !Present(job->key)))
                job->cancelled.store(true);
            if (job && job->done.load(std::memory_order_acquire)) {
                const auto publicationStart = Clock::now();
                job->thread.join();
                stats.reservedBytes           = 0;
                stats.preparationMilliseconds = job->milliseconds;
                if (!job->cancelled.load() && job->document == context.document && Present(job->key)) {
                    stats.bytes += job->data->bvh.GetMemoryUsage();
                    stats.triangles += job->data->bvh.GetElementCount();
                    stats.degenerateTriangles += job->data->degenerate;
                    cache.emplace(job->key, Entry{job->data, ++use, job->request});
                    ++stats.builds;
                    ++indexGeneration;
                    stats.decodeMilliseconds     = job->data->decodeMilliseconds;
                    stats.allocationMilliseconds = job->data->bvh.GetBuildStatistics().copyMilliseconds;
                    stats.boundsMilliseconds     = job->data->bvh.GetBuildStatistics().boundsMilliseconds;
                    stats.partitionMilliseconds  = job->data->bvh.GetBuildStatistics().partitionMilliseconds;
                } else
                    ++stats.discarded;
                job.reset();
                stats.publicationMilliseconds = Milliseconds(publicationStart);
            }
            if (job || !wanted)
                return;
            const auto payload = std::move(wanted);
            const auto key     = GeometryKey(*payload);
            if (!Present(key) || cache.contains(key))
                return;
            const uint64_t reserve = TriangleCount(*payload) * (2 * sizeof(PickTriangle) + 2 * sizeof(BvhNode)) +
                                     Bvh<PickTriangle>::BuildScratchBytes(TriangleCount(*payload)) + payload->bytes.size();
            while (!cache.empty() && (cache.size() >= entryBudget || reserve > budget - stats.bytes)) {
                auto victim = cache.end();
                for (auto it = cache.begin(); it != cache.end(); ++it)
                    if (it->second.request != wantedRequest && it->second.data.use_count() == 1 &&
                        (victim == cache.end() || it->second.used < victim->second.used))
                        victim = it;
                if (victim == cache.end())
                    break;
                stats.bytes -= victim->second.data->bvh.GetMemoryUsage();
                stats.triangles -= victim->second.data->bvh.GetElementCount();
                cache.erase(victim);
                ++stats.evictions;
            }
            if (reserve > budget - stats.bytes || cache.size() >= entryBudget) {
                rejected        = key;
                rejectedRequest = wantedRequest;
                ++indexGeneration;
                return;
            }
            rejected.reset();
            stats.reservedBytes = reserve;
            stats.peakMeshBytes = std::max(stats.peakMeshBytes, stats.bytes + reserve);
            job                 = std::make_unique<Job>();
            job->key            = key;
            job->document       = context.document;
            job->scene          = context.scene;
            job->request        = wantedRequest;
            job->payload        = payload;
            job->thread         = std::thread([work = job.get()]() {
                const auto start = Clock::now();
                Build(*work->payload, *work->data, work->cancelled);
                work->milliseconds = Milliseconds(start);
                work->done.store(true, std::memory_order_release);
            });
        }
    };

    CpuViewportPicker::CpuViewportPicker(uint64_t byteBudget, size_t entryBudget) : impl(std::make_unique<Impl>())
    {
        impl->budget      = byteBudget;
        impl->entryBudget = entryBudget;
    }
    CpuViewportPicker::~CpuViewportPicker() = default;
    void CpuViewportPicker::Reset()
    {
        impl->job.reset();
        impl->wanted.reset();
        impl->rejected.reset();
        impl->scene.reset();
        impl->cache.clear();
        impl->requests.clear();
        impl->direct = {};
        impl->instances.clear();
        impl->instancePartBytes = impl->instanceMessageBytes = 0;
        impl->freeInstances.clear();
        impl->instanceMap.clear();
        impl->geometryUsers.clear();
        impl->unknown.clear();
        impl->unknownList.clear();
        impl->index.Clear();
        impl->syncing.reset();
        impl->appliedVersion = 0;
        impl->indexError.clear();
        ++impl->indexGeneration;
        impl->projections.clear();
        impl->proxies.clear();
        impl->customGeometries.clear();
        impl->preparedProviders = 0;
        impl->context           = {};
        impl->stats             = {};
        ++impl->serial;
    }
    void CpuViewportPicker::Prepare(std::shared_ptr<const aurora::SceneGeometrySnapshot> scene, ViewportPickContext context)
    {
        if (context.document != impl->context.document) {
            impl->wanted.reset();
            impl->rejected.reset();
            for (auto &[key, entry] : impl->cache)
                entry.request = 0;
        }
        if (context.document != impl->context.document) {
            impl->instancePartBytes = impl->instanceMessageBytes = 0;
            impl->index.Clear();
            impl->instances.clear();
            impl->freeInstances.clear();
            impl->instanceMap.clear();
            impl->geometryUsers.clear();
            impl->unknown.clear();
            impl->unknownList.clear();
            impl->requests.clear();
            impl->direct = {};
            impl->syncing.reset();
            impl->appliedVersion = 0;
            ++impl->indexGeneration;
        }
        impl->scene = std::move(scene);
        impl->SyncIndex(impl->scene);
        impl->PrepareProviders();
        context.providers = impl->providers->generation;
        context.scene     = impl->scene ? impl->scene->revision : 0;
        impl->context     = context;
        ++impl->serial;
        impl->Prepare();
        impl->queryAllowance = 1.0;
        if (!impl->requests.empty()) {
            auto next = impl->requests.upper_bound(impl->nextView);
            if (next == impl->requests.end())
                next = impl->requests.begin();
            impl->nextView = next->first;
        }
    }
    ViewportPickContext CpuViewportPicker::Context() const
    {
        auto context      = impl->context;
        context.providers = impl->providers->generation;
        return context;
    }
    uint64_t CpuViewportPicker::PreparationSerial() const
    {
        return impl->serial;
    }
    ViewportPickStatistics CpuViewportPicker::Statistics() const
    {
        return impl->stats;
    }

    ViewportPickRegistration CpuViewportPicker::RegisterProvider(std::shared_ptr<IViewportPickProvider> provider,
                                                                 std::shared_ptr<const void>            moduleLease)
    {
        if (!provider || !provider->Id())
            return {{}, "Provider identity must be nonzero"};
        if (provider->RequiresModuleLease() && !moduleLease)
            return {{}, "Dynamic provider requires a module lease"};
        const auto id    = provider->Id();
        auto      &state = *impl->providers;
        if (state.entries.contains(id))
            return {{}, "Duplicate provider identity"};
        if (state.entries.size() >= 32)
            return {{}, "Provider capacity exceeded"};
        const auto generation = ++state.next;
        state.entries.emplace(id, Impl::ProviderEntry{std::move(moduleLease), std::move(provider), {}, generation});
        ++state.generation;
        auto scope        = std::make_shared<Impl::ProviderScope>();
        scope->owner      = impl->providers;
        scope->id         = id;
        scope->generation = generation;
        return {std::move(scope), {}};
    }
    std::vector<ViewportProxyLayout> CpuViewportPicker::Overlay(const EditorCamera &camera, Vector2 extent) const
    {
        std::vector<ViewportProxyLayout> result;
        if (impl->preparedProviders != impl->providers->generation || impl->proxyStatus != ViewportPickStatus::HIT)
            return result;
        if (impl->proxies.empty())
            return result;
        const auto &projection = impl->ProjectProxies(camera, extent);
        if (projection.failed || projection.next != impl->proxies.size())
            return result;
        std::vector<size_t> ordered;
        for (size_t i = 0; i < impl->proxies.size(); ++i) {
            const auto &proxy = impl->proxies[i];
            if (proxy.depth == ViewportProxyDepth::OVERLAY && proxy.visibilityMask && projection.layouts[i].count)
                ordered.push_back(i);
        }
        // Paint the same stable winner last; pointer distance cannot define a painter order.
        std::ranges::sort(ordered, [&](size_t a, size_t b) {
            return OverlayKey(impl->proxies[a].priority, impl->proxies[a].target) > OverlayKey(impl->proxies[b].priority, impl->proxies[b].target);
        });
        for (const auto i : ordered)
            result.push_back(projection.layouts[i]);
        return result;
    }

    ViewportSelectionBounds CpuViewportPicker::SelectionBounds(const Uuid &actor) const
    {
        ViewportSelectionBounds result;
        const auto              merge = [&](const AABB &bounds) {
            if (ValidBounds(bounds))
                result.bounds = result.bounds ? Merge(*result.bounds, bounds) : bounds;
        };
        const auto found = impl->instanceMap.find(actor);
        if (found != impl->instanceMap.end()) {
            const auto &source = impl->instances[found->second].source;
            if (source.bounds)
                merge(AABB::Transform(*source.bounds, source.world));
            if (source.resources) {
                bool supported = source.state == aurora::SceneGeometryState::READY && !source.resources->parts.empty();
                for (const auto &part : source.resources->parts)
                    supported &= aurora::SupportsSceneHighlight(*source.resources, part);
                result.surface = supported;
                if (!result.bounds)
                    for (const auto &payload : source.resources->payloads)
                        if (payload.mesh && payload.metadata.bounds)
                            merge(AABB::Transform(*payload.metadata.bounds, source.world));
            }
        }
        if (impl->preparedProviders == impl->providers->generation && impl->proxyStatus == ViewportPickStatus::HIT) {
            for (const auto &owned : impl->customGeometries)
                if (owned.source.target.actor == actor && owned.source.bounds) {
                    merge(*owned.source.bounds);
                    result.surface = false;
                }
            for (const auto &proxy : impl->proxies)
                if (proxy.target.actor == actor) {
                    if (const auto *line = std::get_if<ViewportSegmentProxy>(&proxy.shape))
                        merge({Min(line->a, line->b), Max(line->a, line->b)});
                    if (const auto *triangle = std::get_if<ViewportTriangleProxy>(&proxy.shape))
                        merge({Min(Min(triangle->a, triangle->b), triangle->c), Max(Max(triangle->a, triangle->b), triangle->c)});
                    result.surface = false;
                }
        }
        return result;
    }

    ViewportPickResult CpuViewportPicker::Submit(const ViewportPickRequest &request)
    {
        const auto key   = std::pair{request.context.document, request.context.surface};
        auto       found = impl->requests.find(key);
        if (!request.context.request || request.include.size() > 1024 || request.ignore.size() > 1024 || request.locked.size() > 1024)
            return {.context = request.context,
                    .status  = ViewportPickStatus::ERROR,
                    .message = "Invalid or oversized picking request",
                    .reason  = ViewportPickReason::INVALID_REQUEST};
        if (found != impl->requests.end() && found->second.request.context.request == request.context.request) {
            if (!SameViewportPickRequest(found->second.request, request))
                return {.context = request.context,
                        .status  = ViewportPickStatus::ERROR,
                        .message = "Picking token reused with different data",
                        .reason  = ViewportPickReason::INVALID_REQUEST};
            return {.context = request.context};
        }
        if (found != impl->requests.end() && found->second.request.context.request > request.context.request)
            return {.context = request.context, .status = ViewportPickStatus::ERROR, .message = "", .reason = ViewportPickReason::STALE_REQUEST};
        if (found == impl->requests.end() && impl->requests.size() >= 8)
            return {.context = request.context,
                    .status  = ViewportPickStatus::ERROR,
                    .message = "Picking viewport capacity exceeded",
                    .reason  = ViewportPickReason::RESOURCE_LIMIT};
        impl->requests[key] = {.request = request};
        return {.context = request.context};
    }
    ViewportPickResult CpuViewportPicker::Poll(const ViewportPickContext &token)
    {
        const auto found = impl->requests.find({token.document, token.surface});
        if (found == impl->requests.end() || found->second.request.context != token || token.document != impl->context.document ||
            token.scene != impl->context.scene || token.providers != impl->providers->generation)
            return {.context = token, .status = ViewportPickStatus::ERROR, .message = "", .reason = ViewportPickReason::STALE_REQUEST};
        auto &slot = found->second;
        if (slot.result)
            return *slot.result;
        if (impl->queryAllowance <= 0 || (impl->requests.size() > 1 && found->first != impl->nextView))
            return {.context = token, .status = ViewportPickStatus::PENDING};
        const auto started    = Clock::now();
        impl->activeTraversal = &slot.traversal;
        auto result           = Query(slot.request);
        impl->activeTraversal = nullptr;
        impl->queryAllowance -= Milliseconds(started);
        auto next = std::next(found);
        if (next == impl->requests.end())
            next = impl->requests.begin();
        impl->nextView = next->first;
        if (result.status != ViewportPickStatus::PENDING)
            slot.result = result;
        return result;
    }
    void CpuViewportPicker::Cancel(const ViewportPickContext &token)
    {
        const auto found = impl->requests.find({token.document, token.surface});
        if (found != impl->requests.end() && found->second.request.context == token)
            impl->requests.erase(found);
    }
    ViewportPickResult CpuViewportPicker::Query(const ViewportPickRequest &request)
    {
        using namespace aurora;
        auto      &state = *impl;
        const auto start = Clock::now();
        const auto fail  = [&](ViewportPickStatus status, const char *message, ViewportPickReason reason = ViewportPickReason::NONE) {
            return ViewportPickResult{.context = request.context, .status = status, .message = message, .reason = reason};
        };
        if (!state.scene || request.context.document != state.context.document || request.context.scene != state.context.scene || state.syncing)
            return fail(ViewportPickStatus::PENDING, "Picking: preparing instance index");
        if ((!state.providers->entries.empty() || request.context.providers) && request.context.providers != state.providers->generation)
            return fail(ViewportPickStatus::ERROR, "", ViewportPickReason::STALE_REQUEST);
        if (state.preparedProviders != state.providers->generation)
            return fail(ViewportPickStatus::PENDING, "Picking: preparing providers");
        if (state.proxyStatus != ViewportPickStatus::HIT)
            return fail(state.proxyStatus, state.proxyError.c_str(), state.proxyReason);
        if (!state.indexError.empty())
            return fail(ViewportPickStatus::ERROR, state.indexError.c_str(), state.indexReason);
        if (request.include.size() > 1024 || request.ignore.size() > 1024 || request.locked.size() > 1024 || !request.context.document ||
            !request.context.request || !Finite(request.origin) || !Finite(request.direction) || std::abs(request.direction.Length() - 1) > 1.e-4f ||
            !std::isfinite(request.nearDistance) || !std::isfinite(request.farDistance) || request.nearDistance < 0 ||
            request.farDistance < request.nearDistance)
            return fail(ViewportPickStatus::ERROR, "Invalid viewport ray or clipping range", ViewportPickReason::INVALID_REQUEST);
        if (request.view.available) {
            ViewportPickRequest expected;
            if (!MakeViewportPickRequest(request.view.camera, request.view.point, request.view.extent, request.view.imageWidth,
                                         request.view.imageHeight, request.context, expected) ||
                expected.view.pixelX != request.view.pixelX || expected.view.pixelY != request.view.pixelY ||
                (expected.origin - request.origin).Length() > 1.e-5f || (expected.direction - request.direction).Length() > 1.e-5f)
                return fail(ViewportPickStatus::ERROR, "Inconsistent viewport view snapshot", ViewportPickReason::INVALID_REQUEST);
        }
        if (request.policy == ViewportPickPolicy::VISIBLE_SURFACE)
            return fail(ViewportPickStatus::UNSUPPORTED, "Visible-surface picking requires a GPU backend", ViewportPickReason::UNSUPPORTED_COVERAGE);
        Impl::Projection *projection = nullptr;
        if (request.view.available && request.policy == ViewportPickPolicy::EDITOR_DEFAULT && !state.proxies.empty()) {
            projection = &state.ProjectProxies(request.view.camera, request.view.extent);
            if (projection->failed)
                return fail(ViewportPickStatus::ERROR, "Proxy projection budget exceeded", ViewportPickReason::RESOURCE_LIMIT);
            if (projection->next != state.proxies.size())
                return fail(ViewportPickStatus::PENDING, "Picking: preparing view proxy index");
        }
        auto &work = state.activeTraversal ? *state.activeTraversal : state.direct;
        if (!work.initialized || work.generation != state.indexGeneration || work.projection != (projection ? projection->serial : 0) ||
            !SameViewportPickRequest(work.request, request) || (work.done && !state.activeTraversal)) {
            work                    = {};
            work.initialized        = true;
            work.request            = request;
            work.generation         = state.indexGeneration;
            work.projection         = projection ? projection->serial : 0;
            work.result             = {.context = request.context, .status = ViewportPickStatus::MISS, .distance = request.farDistance};
            work.nearest            = request.farDistance;
            work.unresolvedDistance = INFINITY;
        }
        if (work.done)
            return work.result;
        if (state.rejectedRequest != request.context.request)
            state.rejected.reset();
        const auto contains = [](const std::vector<Uuid> &values, const Uuid &id) {
            return std::find(values.begin(), values.end(), id) != values.end();
        };
        const auto defer = [&](float distance, ViewportPickStatus status, const std::string &message,
                               ViewportPickReason reason = ViewportPickReason::NONE) {
            const auto priority = [](ViewportPickStatus value) {
                return value == ViewportPickStatus::ERROR ? 3 : value == ViewportPickStatus::UNSUPPORTED ? 2 : 1;
            };
            if (distance < work.unresolvedDistance || (distance == work.unresolvedDistance && priority(status) > priority(work.unresolved))) {
                work.unresolvedDistance = distance;
                work.unresolved         = status;
                work.diagnostic         = message;
                work.unresolvedReason   = reason;
            }
        };
        const auto nextInstance = [&] {
            work.current = ViewportPickIndex::NONE;
            work.part    = 0;
            work.geometry.reset();
            work.meshCursor = {};
        };
        uint32_t remaining = 256;
        do {
            if (!remaining)
                remaining = 256;
            const float tolerance = std::max(1.e-5f, std::abs(work.nearest) * 1.e-6f);
            const float limit     = std::min(request.farDistance, work.nearest + tolerance);
            uint32_t    proxyId   = ViewportPickIndex::NONE;
            if (request.policy == ViewportPickPolicy::EDITOR_DEFAULT && !work.proxyDone) {
                if (projection) {
                    float entry = 0;
                    if (!projection->index.Next({request.view.point.x, request.view.point.y, 1}, {0, 0, -1}, 0, 2, work.proxyCursor, remaining,
                                                proxyId, entry, state.stats.proxyNodeVisits)) {
                        if (!work.proxyCursor.done)
                            continue;
                        work.proxyDone = true;
                    }
                } else if (work.proxyAt < state.proxies.size())
                    proxyId = uint32_t(work.proxyAt++);
                else
                    work.proxyDone = true;
            }
            if (proxyId != ViewportPickIndex::NONE) {
                const auto &proxy = state.proxies[proxyId];
                if (!(proxy.visibilityMask & request.visibilityMask) || contains(request.ignore, proxy.target.actor))
                    continue;
                const bool selectable = proxy.selectable && !contains(request.locked, proxy.target.actor) &&
                                        (request.include.empty() || contains(request.include, proxy.target.actor));
                if (!selectable && !proxy.occluder)
                    continue;
                float                  distance = 0, screenDistance = 0;
                std::optional<Vector3> position;
                if (!HitViewportPickProxy(proxy, request, distance, screenDistance, position))
                    continue;
                const bool overlay = proxy.depth == ViewportProxyDepth::OVERLAY;
                if (work.overlay && !overlay)
                    continue;
                if (overlay && work.overlay) {
                    if (OverlayKey(proxy.priority, proxy.target) >= OverlayKey(work.priority, work.result.target))
                        continue;
                } else if (!overlay) {
                    if (!work.tiePass) {
                        if (work.surface && distance >= work.nearest)
                            continue;
                        work.nearest = distance;
                    } else if (distance > limit ||
                               (work.surface && std::tuple{-int64_t(proxy.priority), screenDistance, TargetKey(proxy.target)} >=
                                                    std::tuple{-int64_t(work.priority), work.screenDistance, TargetKey(work.result.target)}))
                        continue;
                }
                work.overlay        = overlay;
                work.surface        = true;
                work.priority       = proxy.priority;
                work.screenDistance = screenDistance;
                work.result         = {.context   = request.context,
                                       .status    = selectable ? ViewportPickStatus::HIT : ViewportPickStatus::MISS,
                                       .actor     = selectable ? proxy.target.actor : Uuid{},
                                       .part      = proxy.target.part,
                                       .distance  = distance,
                                       .position  = position,
                                       .target    = proxy.target,
                                       .semantics = ViewportPickSemantics::EDITOR_PROXY,
                                       .reason    = selectable ? ViewportPickReason::NONE : ViewportPickReason::OCCLUDED};
                continue;
            }
            if (work.overlay) {
                work.done = true;
                break;
            }
            if (work.current == ViewportPickIndex::NONE) {
                if (!state.index.Next(request.origin, request.direction, request.nearDistance, limit, work.cursor, remaining, work.current,
                                      work.entry, state.stats.nodeVisits)) {
                    if (!work.cursor.done)
                        continue;
                    if (work.unknownAt < state.unknownList.size()) {
                        work.current = state.unknownList[work.unknownAt++];
                        work.entry   = request.nearDistance;
                    } else {
                        if (work.customAt < state.customGeometries.size()) {
                            if (!remaining)
                                continue;
                            const auto &owned      = state.customGeometries[work.customAt];
                            const auto &source     = owned.source;
                            const auto  nextCustom = [&] {
                                work.customQuery.reset();
                                ++work.customAt;
                            };
                            --remaining;
                            float      entry      = request.nearDistance;
                            const bool selectable = source.selectable && !contains(request.locked, source.target.actor) &&
                                                    (request.include.empty() || contains(request.include, source.target.actor));
                            if (!(source.visibilityMask & request.visibilityMask) || contains(request.ignore, source.target.actor) ||
                                (!selectable && !source.occluder) ||
                                (source.bounds && !Bounds(*source.bounds, request.origin, request.direction, request.nearDistance, limit, entry))) {
                                nextCustom();
                                continue;
                            }
                            if (!work.customQuery) {
                                work.customQuery              = std::make_unique<Impl::Traversal::CustomQuery>();
                                work.customQuery->moduleLease = owned.moduleLease;
                                work.customQuery->geometry    = source.geometry;
                            }
                            const double               allowance = state.activeTraversal ? state.queryAllowance : 1.0;
                            ViewportPickGeometryBudget budget{remaining, std::max(0.0, allowance - Milliseconds(start))};
                            if (!budget.work || budget.milliseconds <= 0)
                                return fail(ViewportPickStatus::PENDING, "Picking: continuing custom geometry query");
                            auto result = work.customQuery->geometry->Raycast({request.origin, request.direction, request.nearDistance, limit},
                                                                              budget, work.customQuery->cursor);
                            if (budget.work > remaining || result.message.size() > 4096 ||
                                (result.status != ViewportPickStatus::HIT && result.status != ViewportPickStatus::MISS &&
                                 result.status != ViewportPickStatus::PENDING && result.status != ViewportPickStatus::UNSUPPORTED &&
                                 result.status != ViewportPickStatus::ERROR)) {
                                result = {.status  = ViewportPickStatus::ERROR,
                                          .reason  = ViewportPickReason::INVALID_REQUEST,
                                          .message = "Invalid custom geometry query result or budget"};
                            } else
                                remaining = budget.work;
                            if (result.status == ViewportPickStatus::PENDING)
                                return fail(result.status,
                                            result.message.empty() ? "Picking: continuing custom geometry query" : result.message.c_str(),
                                            result.reason);
                            if (result.status == ViewportPickStatus::HIT) {
                                const auto  point   = request.origin + request.direction * result.distance;
                                const float error   = std::max(1.e-5f, std::abs(result.distance) * 1.e-5f);
                                float       ignored = 0;
                                if (!std::isfinite(result.distance) || result.distance < request.nearDistance || result.distance > limit ||
                                    !Finite(point) ||
                                    (result.position && (!Finite(*result.position) || (*result.position - point).Length() > error)) ||
                                    (result.normal && (!Finite(*result.normal) || std::abs(result.normal->Length() - 1.f) > 1.e-3f)) ||
                                    (source.bounds && !Bounds(*source.bounds, request.origin, request.direction, result.distance - error,
                                                              result.distance + error, ignored))) {
                                    result = {.status  = ViewportPickStatus::ERROR,
                                              .reason  = ViewportPickReason::INVALID_REQUEST,
                                              .message = "Custom geometry returned an invalid world-space hit"};
                                }
                            }
                            if (result.status == ViewportPickStatus::ERROR || result.status == ViewportPickStatus::UNSUPPORTED) {
                                defer(entry, result.status, result.message, result.reason);
                            } else if (result.status == ViewportPickStatus::HIT) {
                                auto target       = source.target;
                                target.instance   = result.instance;
                                target.part       = result.part;
                                target.subelement = result.subelement;
                                const bool better = !work.surface || (!work.tiePass ? result.distance < work.nearest
                                                                                    : std::tuple{-int64_t(source.priority), 0.f, TargetKey(target)} <
                                                                                          std::tuple{-int64_t(work.priority), work.screenDistance,
                                                                                                     TargetKey(work.result.target)});
                                if (better) {
                                    if (!work.tiePass)
                                        work.nearest = result.distance;
                                    work.surface        = true;
                                    work.priority       = source.priority;
                                    work.screenDistance = 0;
                                    work.result         = {.context  = request.context,
                                                           .status   = selectable ? ViewportPickStatus::HIT : ViewportPickStatus::MISS,
                                                           .actor    = selectable ? target.actor : Uuid{},
                                                           .part     = target.part,
                                                           .distance = result.distance,
                                                           .position = result.position,
                                                           .normal   = result.normal,
                                                           .message  = result.message,
                                                           .target   = target,
                                                           .reason   = selectable ? ViewportPickReason::NONE : ViewportPickReason::OCCLUDED};
                                }
                            }
                            nextCustom();
                            continue;
                        }
                        if (work.unresolved != ViewportPickStatus::MISS && work.unresolvedDistance <= limit) {
                            work.result = fail(work.unresolved, work.diagnostic.c_str(), work.unresolvedReason);
                            if (work.unresolved == ViewportPickStatus::UNSUPPORTED)
                                work.result.reason = ViewportPickReason::UNSUPPORTED_COVERAGE;
                        } else if (!work.tiePass && work.surface) {
                            work.tiePass     = true;
                            work.proxyAt     = 0;
                            work.proxyCursor = {};
                            work.proxyDone   = false;
                            work.cursor      = {};
                            work.unknownAt   = 0;
                            work.surface     = false;
                            work.customAt    = 0;
                            work.customQuery.reset();
                            continue;
                        }
                        work.done = true;
                        break;
                    }
                }
                ++state.stats.candidateTests;
            }
            auto       &instance = state.instances[work.current];
            const auto &source   = instance.source;
            if (!(source.visibilityMask & request.visibilityMask) || contains(request.ignore, source.actor)) {
                nextInstance();
                continue;
            }
            const bool selectable =
                source.selectable && !contains(request.locked, source.actor) && (request.include.empty() || contains(request.include, source.actor));
            if (!selectable && !source.occluder) {
                nextInstance();
                continue;
            }
            if (!instance.valid) {
                defer(work.entry, ViewportPickStatus::ERROR, "Mesh has a singular or invalid world transform");
                nextInstance();
                continue;
            }
            if (source.state != SceneGeometryState::READY || !source.resources) {
                defer(work.entry,
                      source.state == SceneGeometryState::PENDING       ? ViewportPickStatus::PENDING
                      : source.state == SceneGeometryState::UNSUPPORTED ? ViewportPickStatus::UNSUPPORTED
                                                                        : ViewportPickStatus::ERROR,
                      source.message.empty() ? "Picking: waiting for mesh resources" : source.message);
                nextInstance();
                continue;
            }
            if (work.part == source.resources->parts.size()) {
                nextInstance();
                continue;
            }
            const auto &part     = source.resources->parts[work.part];
            const auto &prepared = instance.parts[work.part];
            const auto  origin   = TransformPoint(instance.inverse, request.origin, 1),
                       direction = TransformPoint(instance.inverse, request.direction, 0);
            const auto nextPart  = [&] {
                ++work.part;
                work.geometry.reset();
                work.meshCursor = {};
            };
            if (!Finite(origin) || !Finite(direction)) {
                defer(work.entry, ViewportPickStatus::ERROR, "Invalid inverse ray");
                nextInstance();
                continue;
            }
            if (!prepared.payload) {
                defer(work.entry, ViewportPickStatus::ERROR, "Missing owned mesh payload");
                nextPart();
                continue;
            }
            float entry = work.entry;
            if (prepared.payload->metadata.bounds &&
                !Bounds(*prepared.payload->metadata.bounds, origin, direction, request.nearDistance, limit, entry)) {
                nextPart();
                continue;
            }
            if (!part.surface.SupportsGeometry()) {
                defer(entry, ViewportPickStatus::UNSUPPORTED, "Picking requires a declared static opaque surface contract");
                nextPart();
                continue;
            }
            if (!work.geometry) {
                const auto found = state.cache.find(prepared.key);
                if (found == state.cache.end()) {
                    if (state.rejected && *state.rejected == prepared.key)
                        defer(entry, ViewportPickStatus::ERROR, "Picking geometry exceeds the bounded cache budget",
                              ViewportPickReason::RESOURCE_LIMIT);
                    else {
                        if (!state.wanted) {
                            state.wanted        = prepared.payload;
                            state.wantedRequest = request.context.request;
                        }
                        defer(entry, ViewportPickStatus::PENDING, "Picking: preparing triangle BVH");
                    }
                    nextPart();
                    continue;
                }
                found->second.used    = ++state.use;
                found->second.request = request.context.request;
                work.geometry         = found->second.data;
                ++state.stats.cacheHits;
            }
            if (work.geometry->status != ViewportPickStatus::HIT) {
                defer(entry, work.geometry->status, work.geometry->message);
                nextPart();
                continue;
            }
            const bool complete =
                work.geometry->bvh.QueryRayStep(origin, direction, limit, work.meshCursor, remaining, [&](const PickTriangle &triangle) {
                    ++state.stats.triangleTests;
                    float distance = 0;
                    if (triangle.part != part.part || !Intersect(triangle, origin, direction, distance) || distance < request.nearDistance ||
                        distance > limit)
                        return;
                    const auto  position      = TransformPoint(source.world, origin + direction * distance, 1);
                    const float worldDistance = (position - request.origin).Dot(request.direction);
                    if (!Finite(position) || !std::isfinite(worldDistance) || worldDistance < request.nearDistance || worldDistance > limit)
                        return;
                    auto normal = (TransformPoint(source.world, triangle.b, 1) - TransformPoint(source.world, triangle.a, 1))
                                      .Cross(TransformPoint(source.world, triangle.c, 1) - TransformPoint(source.world, triangle.a, 1));
                    const bool front = (normal.Dot(request.direction) < 0) == part.surface.counterClockwise;
                    if ((part.surface.cull == SceneSurfaceCull::BACK && !front) || (part.surface.cull == SceneSurfaceCull::FRONT && front))
                        return;
                    const ViewportPickTarget target{source.actor, source.component, 0, source.generation, 0, part.part, 0};
                    if (!work.tiePass) {
                        if (work.surface && worldDistance >= work.nearest)
                            return;
                        work.nearest = worldDistance;
                    } else if (work.surface && std::tuple{int64_t{0}, 0.f, TargetKey(target)} >=
                                                   std::tuple{-int64_t(work.priority), work.screenDistance, TargetKey(work.result.target)})
                        return;
                    work.surface        = true;
                    work.priority       = 0;
                    work.screenDistance = 0;
                    const float length  = normal.Length();
                    work.result         = {.context  = request.context,
                                           .status   = selectable ? ViewportPickStatus::HIT : ViewportPickStatus::MISS,
                                           .actor    = selectable ? source.actor : Uuid{},
                                           .part     = part.part,
                                           .distance = worldDistance,
                                           .position = position,
                                           .normal   = length > 0 ? std::optional<Vector3>(normal / length) : std::nullopt,
                                           .message  = work.geometry->message,
                                           .target   = target,
                                           .reason   = selectable ? ViewportPickReason::NONE : ViewportPickReason::OCCLUDED};
                });
            if (complete)
                nextPart();
        } while (Milliseconds(start) < (state.activeTraversal ? std::max(0.01, state.queryAllowance) : 1.0));
        state.stats.queryMilliseconds = Milliseconds(start);
        if (!work.done)
            return fail(ViewportPickStatus::PENDING, "Picking: continuing bounded scene query");
        work.geometry.reset();
        return work.result;
    }
} // namespace sky::editor
