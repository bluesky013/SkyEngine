#pragma once

#include <editor/core/preferences/PreferencePageRegistry.h>
#include <framework/serialization/Any.h>

#include <functional>
#include <memory>

namespace sky::editor {

    // A native host supplies its path/atomic replacement adapter separately.
    // Read returns NotFound for absent storage. Failed Write MUST preserve its
    // previous document. Calls on one store are serialized by its owner.
    class IPreferenceStorage {
    public:
        virtual ~IPreferenceStorage()                               = default;
        virtual PreferenceResult Read(std::string &document)        = 0;
        virtual PreferenceResult Write(const std::string &document) = 0;
    };

    struct PreferenceValue {
        PreferencePage identity;
        Any            value;
        PreferencePage View() const
        {
            auto page   = identity;
            page.object = const_cast<void *>(value.Data());
            return page;
        }
    };

    // UI-free helpers use owned values and recheck registration identity after
    // every callback. Reflection metadata/live objects remain borrowed; DLL
    // unloading and concurrent mutation are outside this main-thread contract.
    PreferenceResult CapturePreferenceValues(const std::vector<PreferencePage> &pages,
                                             std::vector<PreferenceValue>      &values,
                                             const std::function<bool()>       &isCurrent = {});
    PreferenceResult ValidatePreferenceValues(PreferencePageRegistry             &registry,
                                              const std::vector<PreferenceValue> &values,
                                              const std::function<bool()>        &isCurrent = {});
    PreferenceResult PublishPreferenceValues(PreferencePageRegistry             &registry,
                                             const std::vector<PreferenceValue> &values,
                                             const std::function<bool()>        &isCurrent = {});

    class PreferencesPersistence {
    public:
        explicit PreferencesPersistence(std::shared_ptr<IPreferenceStorage> storage);
        ~PreferencesPersistence();
        PreferencesPersistence(const PreferencesPersistence &)            = delete;
        PreferencesPersistence &operator=(const PreferencesPersistence &) = delete;

        // Load only at a host-controlled boundary with no open editing session.
        // Missing fields/pages preserve provider values. Decode/validation errors
        // publish nothing; setter/application errors report any partial effects.
        PreferenceResult Load(PreferencePageRegistry &registry);
        // Merge known fields into a fresh read, retaining unknown object keys and
        // absent plugin pages. Known arrays are replaced. No live writes here.
        // The caller validates candidates before Save when used as Apply's gate.
        PreferenceResult Save(const std::vector<PreferenceValue> &values, const std::function<bool()> &isCurrent = {});

    private:
        struct State;
        std::shared_ptr<State> state;
    };

} // namespace sky::editor
