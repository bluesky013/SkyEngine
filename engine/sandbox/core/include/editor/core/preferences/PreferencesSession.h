#pragma once

#include <editor/core/command/CommandService.h>
#include <editor/core/preferences/PreferencePageRegistry.h>

#include <memory>

namespace sky::editor {

    class PreferencesPersistence;

    // UI-free, dialog-local working copies and history. The registry and its
    // reflection types must outlive the session. Bound views must detach before
    // Begin, End or Synchronize can release their former working objects.
    class PreferencesSession {
    public:
        PreferencesSession();
        ~PreferencesSession();
        PreferencesSession(const PreferencesSession &)            = delete;
        PreferencesSession &operator=(const PreferencesSession &) = delete;

        void Begin(PreferencePageRegistry *registry);
        void End(); // discard unapplied changes and history
        bool IsOpen() const;

        bool MatchesRegistry() const;
        // Reorder preserves working objects/history. Add/remove/re-registration
        // clears history before releasing old storage; new pages start fresh.
        // Reflected copies may reenter: interrupted synchronization keeps the
        // replacement session, and an in-flight copy retains its source page.
        void                  Synchronize();
        std::size_t           GetPageCount() const;
        const PreferencePage *GetPage(std::size_t index) const;

        // Writes reflected writable members to still-matching live registrations
        // and starts a new Apply baseline. Returns false for a changed registry;
        // the host must detach, Synchronize and bind again before retrying.
        bool Apply();
        // Validate -> optional durable Save -> guarded live/application writes.
        // Only complete success clears history. A post-save failure explicitly
        // reports storage/live effects; no cross-callback rollback is promised.
        // Callback edits through History(), including pending transaction edits,
        // invalidate the captured operation and preserve the newer work.
        PreferenceResult ApplyDetailed(PreferencesPersistence *persistence = nullptr, const std::function<bool()> &isCurrent = {});
        CommandService  &History();

    private:
        struct State;
        std::shared_ptr<State> state;
    };

} // namespace sky::editor
