#include <editor/core/preferences/PreferencesPersistence.h>
#include <editor/core/preferences/PreferencesSession.h>

#include "PreferenceGuard.h"

#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationFactory.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sky::editor {

    namespace {
        bool CanCopy(const PreferencePage &page)
        {
            return !page.id.empty() && page.registration != 0 && page.object != nullptr && page.type != nullptr && page.type->info != nullptr &&
                   page.type->info->staticInfo != nullptr && page.type->info->copy != nullptr;
        }

        std::vector<const PreferencePage *> EligiblePages(const PreferencePageRegistry *registry)
        {
            std::vector<const PreferencePage *> result;
            if (registry == nullptr) {
                return result;
            }
            std::unordered_map<std::string, std::size_t> counts;
            for (const auto &page : registry->GetPages()) {
                ++counts[page.id];
            }
            for (const auto &page : registry->GetPages()) {
                if (counts[page.id] == 1 && CanCopy(page)) {
                    result.push_back(&page);
                }
            }
            return result;
        }
    } // namespace

    struct PreferencesSession::State {
        struct Page {
            explicit Page(const PreferencePage &source) : live(source), value(Any::Create(live.type->info, live.object)), view(live)
            {
                view.object = value.Data();
            }
            bool Matches(const PreferencePage &page) const
            {
                return live.registration == page.registration && live.id == page.id && live.object == page.object && live.type == page.type &&
                       live.type->info == page.type->info;
            }
            PreferencePage live;
            Any            value;
            PreferencePage view;
        };

        PreferencePageRegistry            *registry   = nullptr;
        bool                               open       = false;
        uint64_t                           generation = 0;
        bool                               applying   = false;
        std::vector<std::shared_ptr<Page>> pages;
        CommandService                     commands; // destroyed before its working-object targets
    };

    PreferencesSession::PreferencesSession() : state(std::make_shared<State>())
    {
    }

    PreferencesSession::~PreferencesSession()
    {
        // In-flight Apply owns State, not this object. Invalidate silently;
        // destruction must not broadcast Clear callbacks through a dying host.
        state->open     = false;
        state->registry = nullptr;
        ++state->generation;
    }

    void PreferencesSession::Begin(PreferencePageRegistry *registry)
    {
        const auto current    = state;
        const auto generation = ++current->generation;
        current->open         = false;
        current->registry     = nullptr;
        current->commands.Clear();
        if (generation != current->generation) {
            return;
        }
        current->pages.clear();
        current->registry = registry;
        current->open     = true;
        Synchronize();
    }

    void PreferencesSession::End()
    {
        const auto current    = state;
        const auto generation = ++current->generation;
        current->open         = false;
        current->registry     = nullptr;
        // History owns raw working-object targets. Destroy it first.
        current->commands.Clear();
        if (generation == current->generation) {
            current->pages.clear();
        }
    }

    bool PreferencesSession::IsOpen() const
    {
        return state->open;
    }
    CommandService &PreferencesSession::History()
    {
        return state->commands;
    }
    std::size_t PreferencesSession::GetPageCount() const
    {
        return state->pages.size();
    }

    const PreferencePage *PreferencesSession::GetPage(std::size_t index) const
    {
        return index < state->pages.size() ? &state->pages[index]->view : nullptr;
    }

    bool PreferencesSession::MatchesRegistry() const
    {
        const auto current = state;
        if (!current->open) {
            return false;
        }
        const auto live = EligiblePages(current->registry);
        if (live.size() != current->pages.size()) {
            return false;
        }
        for (std::size_t i = 0; i < live.size(); ++i) {
            if (!current->pages[i]->Matches(*live[i]) || current->pages[i]->view.title != live[i]->title) {
                return false;
            }
        }
        return true;
    }

    void PreferencesSession::Synchronize()
    {
        const auto current = state;
        if (!current->open) {
            return;
        }
        const auto generation = ++current->generation;
        const auto snapshot   = [&] {
            std::vector<PreferencePage> result;
            for (const auto *page : EligiblePages(current->registry)) {
                result.push_back(*page);
            }
            return result;
        };
        auto       live = snapshot();
        const bool sameIdentities =
            live.size() == current->pages.size() && std::all_of(live.begin(), live.end(), [&current](const auto &page) {
                return std::any_of(current->pages.begin(), current->pages.end(), [&page](const auto &old) { return old->Matches(page); });
            });
        if (!sameIdentities) {
            current->commands.Clear();
            if (generation != current->generation || !current->open) {
                return;
            }
            // Clear notifies observers, which may mutate/reallocate the live
            // registry without replacing this session. Never retain its nodes.
            live = snapshot();
        }
        const auto registry  = current->registry;
        const auto revision  = current->commands.GetRevision();
        const auto unchanged = [&] {
            if (generation != current->generation || !current->open || current->registry != registry || current->commands.GetRevision() != revision) {
                return false;
            }
            const auto registered = EligiblePages(registry);
            if (registered.size() != live.size()) {
                return false;
            }
            for (std::size_t i = 0; i < live.size(); ++i) {
                const auto &a = live[i];
                const auto &b = *registered[i];
                if (a.registration != b.registration || a.id != b.id || a.object != b.object || a.type != b.type) {
                    return false;
                }
            }
            return true;
        };
        if (!unchanged()) {
            return;
        }
        std::vector<std::shared_ptr<State::Page>> next;
        next.reserve(live.size());
        for (const auto &page : live) {
            auto old = std::find_if(current->pages.begin(), current->pages.end(), [&page](const auto &entry) { return entry->Matches(page); });
            if (old != current->pages.end()) {
                next.push_back(*old);
            } else {
                next.push_back(std::make_shared<State::Page>(page));
                // Reflected copy construction can replace the registry/session
                // or edit its history. Keep the original page set intact until
                // every candidate has been built against the same snapshot.
                if (!unchanged()) {
                    return;
                }
            }
        }
        for (std::size_t i = 0; i < live.size(); ++i) {
            next[i]->view.title = live[i].title;
        }
        current->pages = std::move(next);
    }

    bool PreferencesSession::Apply()
    {
        return ApplyDetailed().Succeeded();
    }

    PreferenceResult PreferencesSession::ApplyDetailed(PreferencesPersistence *persistence, const std::function<bool()> &isCurrent)
    {
        const auto current = state;
        if (current->applying) {
            return {PreferenceCode::Busy, "", "", "Preferences are already being applied."};
        }
        current->applying = true;
        struct ApplyScope {
            std::shared_ptr<State> state;
            ~ApplyScope()
            {
                state->applying = false;
            }
        } scope{current};
        const auto generation = current->generation;
        const auto revision   = current->commands.GetRevision();
        const auto validAt    = [current, generation, isCurrent](uint64_t expectedRevision) {
            const auto unchanged = [&] {
                return generation == current->generation && current->open && current->registry != nullptr &&
                       current->commands.GetRevision() == expectedRevision;
            };
            if (!unchanged()) {
                return false;
            }
            if (!detail::CheckPreferenceGuard(isCurrent)) {
                return false;
            }
            // The guard itself can close/reopen the session or edit its work.
            if (!unchanged()) {
                return false;
            }
            const auto live = EligiblePages(current->registry);
            if (live.size() != current->pages.size()) {
                return false;
            }
            for (std::size_t i = 0; i < live.size(); ++i) {
                if (!current->pages[i]->Matches(*live[i])) {
                    return false;
                }
            }
            return true;
        };
        const auto       valid = [validAt, revision] { return validAt(revision); };
        PreferenceResult effects;
        const auto       conflict = [&] {
            auto result    = effects;
            result.code    = PreferenceCode::Conflict;
            result.message = "Preferences changed during application.";
            return result;
        };
        if (!valid()) {
            return conflict();
        }
        auto      *registry = current->registry;
        const auto sources  = current->pages;
        if (registry->GetPages().size() != sources.size()) {
            return {PreferenceCode::InvalidData, "", "", "Preferences contain an invalid or duplicate page."};
        }
        // Keep every source page alive throughout candidate capture, including
        // a copy constructor that calls End/Begin before reading its source.
        std::vector<PreferencePage> sourceViews;
        sourceViews.reserve(sources.size());
        for (const auto &source : sources) {
            auto view   = source->live;
            view.object = source->value.Data();
            sourceViews.push_back(std::move(view));
        }
        std::vector<PreferenceValue> values;
        auto                         result = CapturePreferenceValues(sourceViews, values, valid);
        if (!result.Succeeded()) {
            return result;
        }
        if (!valid() || values.size() != current->pages.size()) {
            return conflict();
        }
        for (std::size_t i = 0; i < values.size(); ++i) {
            values[i].identity = sources[i]->live;
        }
        result = ValidatePreferenceValues(*registry, values, valid);
        if (!result.Succeeded()) {
            return result;
        }
        if (persistence != nullptr) {
            result = persistence->Save(values, valid);
            if (!result.Succeeded()) {
                return result;
            }
            effects.storageWritten = result.storageWritten;
        }
        if (!valid()) {
            return conflict();
        }
        result = PublishPreferenceValues(*registry, values, valid);
        result.storageWritten |= effects.storageWritten;
        effects = result;
        if (!result.Succeeded()) {
            return result;
        }
        if (!valid()) {
            return conflict();
        }
        current->commands.Clear();
        // Clear advances the revision once. Any edit/new history created by
        // its observers must survive and invalidate this completed snapshot.
        return validAt(revision + 1) ? effects : conflict();
    }

} // namespace sky::editor
