#include <editor/core/preferences/PreferencesPersistence.h>
#include <editor/core/property/PropertyEditor.h>

#include <framework/serialization/SerializationFactory.h>

#include "PreferenceGuard.h"

#include <algorithm>
#include <exception>
#include <unordered_set>

namespace sky::editor {
    namespace {
        PreferenceResult Failure(PreferenceCode code, const std::string &page, const std::string &path, const std::string &message)
        {
            return {code, page, path, message};
        }

        PreferenceResult CheckValues(const std::vector<PreferenceValue> &values)
        {
            std::unordered_set<std::string> ids;
            for (const auto &entry : values) {
                const auto &page = entry.identity;
                if (page.id.empty() || !ids.insert(page.id).second || page.registration == 0 || page.object == nullptr || page.type == nullptr ||
                    page.type->info == nullptr || entry.value.Info() != page.type->info || entry.value.Data() == nullptr) {
                    return Failure(PreferenceCode::InvalidData, page.id, "", "Invalid preference candidate.");
                }
            }
            return {};
        }

        bool SamePage(const PreferencePage &a, const PreferencePage &b)
        {
            return a.id == b.id && a.registration == b.registration && a.object == b.object && a.type == b.type;
        }

        bool Current(PreferencePageRegistry &registry, const std::vector<PreferenceValue> &values, const std::function<bool()> &isCurrent)
        {
            if (!detail::CheckPreferenceGuard(isCurrent)) {
                return false;
            }
            const auto &pages = registry.GetPages();
            if (pages.size() != values.size()) {
                return false;
            }
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (!SamePage(pages[i], values[i].identity)) {
                    return false;
                }
            }
            return true;
        }

        PreferenceResult WithEffects(PreferenceResult result, const PreferenceResult &effects)
        {
            result.storageWritten |= effects.storageWritten;
            result.liveWritten |= effects.liveWritten;
            result.applicationApplied |= effects.applicationApplied;
            return result;
        }
    } // namespace

    PreferenceResult
    CapturePreferenceValues(const std::vector<PreferencePage> &pages, std::vector<PreferenceValue> &values, const std::function<bool()> &isCurrent)
    {
        // Reflection copying can run provider code. Own descriptors before the
        // first call so a registry mutation cannot invalidate this iteration.
        const auto                      identities = pages;
        std::unordered_set<std::string> ids;
        std::vector<PreferenceValue>    next;
        next.reserve(identities.size());
        for (const auto &page : identities) {
            if (!detail::CheckPreferenceGuard(isCurrent)) {
                return Failure(PreferenceCode::Conflict, page.id, "", "Preferences changed during value capture.");
            }
            if (page.id.empty() || !ids.insert(page.id).second || page.registration == 0 || page.object == nullptr || page.type == nullptr ||
                page.type->info == nullptr || page.type->info->staticInfo == nullptr || page.type->info->copy == nullptr) {
                return Failure(PreferenceCode::InvalidData, page.id, "", "Preferences contain an invalid or duplicate page.");
            }
            Any value;
            try {
                value = Any::Create(page.type->info, page.object);
            }
            catch (const std::exception &error) {
                return Failure(PreferenceCode::InvalidData, page.id, "", error.what());
            }
            catch (...) {
                return Failure(PreferenceCode::InvalidData, page.id, "", "Could not copy preferences.");
            }
            if (!detail::CheckPreferenceGuard(isCurrent)) {
                return Failure(PreferenceCode::Conflict, page.id, "", "Preferences changed during value capture.");
            }
            next.push_back({page, std::move(value)});
        }
        values = std::move(next);
        return {};
    }

    PreferenceResult
    ValidatePreferenceValues(PreferencePageRegistry &registry, const std::vector<PreferenceValue> &values, const std::function<bool()> &isCurrent)
    {
        const auto checked = CheckValues(values);
        if (!checked.Succeeded()) {
            return checked;
        }
        if (!Current(registry, values, isCurrent)) {
            return Failure(PreferenceCode::Conflict, "", "", "Preferences changed during the operation.");
        }
        for (const auto &value : values) {
            auto callback = value.identity.validate;
            if (callback) {
                PreferenceResult result;
                const auto       candidate = value.View();
                try {
                    result = callback(candidate);
                }
                catch (const std::exception &error) {
                    result = Failure(PreferenceCode::ValidationFailed, value.identity.id, "", error.what());
                }
                catch (...) {
                    result = Failure(PreferenceCode::ValidationFailed, value.identity.id, "", "Preference validation failed.");
                }
                if (!Current(registry, values, isCurrent)) {
                    return Failure(PreferenceCode::Conflict, value.identity.id, "", "Preferences changed during validation.");
                }
                if (!result.Succeeded()) {
                    if (result.pageId.empty())
                        result.pageId = value.identity.id;
                    if (result.message.empty())
                        result.message = "Preference validation failed.";
                    return result;
                }
            }
        }
        return {};
    }

    PreferenceResult
    PublishPreferenceValues(PreferencePageRegistry &registry, const std::vector<PreferenceValue> &values, const std::function<bool()> &isCurrent)
    {
        const auto checked = CheckValues(values);
        if (!checked.Succeeded()) {
            return checked;
        }
        PreferenceResult effects;
        const auto       conflict = [&] {
            return WithEffects(Failure(PreferenceCode::Conflict, "", "", "Preferences changed during application."), effects);
        };
        if (!Current(registry, values, isCurrent)) {
            return conflict();
        }
        struct Write {
            PreferencePage identity;
            std::string    member;
            Any            value;
        };
        std::vector<Write> writes;
        for (const auto &page : values) {
            for (const auto &[name, member] : page.identity.type->members) {
                const auto attributes = ReadPropertyAttributes(member);
                // Preserve the existing session's writable/editable-member
                // contract; isStatic metadata is not reliable in TypeFactory.
                if (member.isConst || member.setterFn == nullptr || attributes.readOnly || !attributes.visible) {
                    continue;
                }
                Any value;
                try {
                    value = member.getterFn != nullptr        ? member.getterFn(const_cast<void *>(page.value.Data()))
                            : member.getterConstFn != nullptr ? member.getterConstFn(page.value.Data())
                                                              : Any{};
                }
                catch (const std::exception &error) {
                    return Failure(PreferenceCode::ApplyFailed, page.identity.id, std::string(name), error.what());
                }
                catch (...) {
                    return Failure(PreferenceCode::ApplyFailed, page.identity.id, std::string(name), "Could not read a preference value.");
                }
                if (!Current(registry, values, isCurrent)) {
                    return conflict();
                }
                if (!value || value.Info() != member.info) {
                    return Failure(PreferenceCode::ApplyFailed, page.identity.id, std::string(name),
                                   "Getter returned an incompatible preference value.");
                }
                writes.push_back({page.identity, std::string(name), std::move(value)});
            }
        }
        for (const auto &write : writes) {
            if (!Current(registry, values, isCurrent)) {
                return conflict();
            }
            // Resolve the descriptor afresh; callbacks can reallocate the registry.
            const auto found = std::find_if(registry.GetPages().begin(), registry.GetPages().end(),
                                            [&](const auto &page) { return SamePage(page, write.identity); });
            if (found == registry.GetPages().end()) {
                return conflict();
            }
            const auto member = found->type->members.find(write.member);
            if (member == found->type->members.end() || member->second.setterFn == nullptr || member->second.info != write.value.Info()) {
                return WithEffects(Failure(PreferenceCode::Conflict, write.identity.id, write.member, "Preference schema changed."), effects);
            }
            auto      *object   = found->object;
            const auto setter   = member->second.setterFn;
            const auto changed  = member->second.valueChangedFn;
            effects.liveWritten = true; // a rejecting/throwing setter may still have effects
            try {
                if (!setter(object, write.value.Data())) {
                    return WithEffects(Failure(PreferenceCode::ApplyFailed, write.identity.id, write.member, "Could not apply a preference value."),
                                       effects);
                }
                if (!Current(registry, values, isCurrent)) {
                    return conflict();
                }
                if (changed != nullptr) {
                    changed(object);
                }
            }
            catch (const std::exception &error) {
                return WithEffects(Failure(PreferenceCode::ApplyFailed, write.identity.id, write.member, error.what()), effects);
            }
            catch (...) {
                return WithEffects(Failure(PreferenceCode::ApplyFailed, write.identity.id, write.member, "Could not apply a preference value."),
                                   effects);
            }
            if (!Current(registry, values, isCurrent)) {
                return conflict();
            }
        }
        for (const auto &value : values) {
            if (!Current(registry, values, isCurrent)) {
                return conflict();
            }
            auto callback = value.identity.apply;
            if (!callback) {
                continue;
            }
            // This is a live descriptor, never the disposable working snapshot.
            const auto live            = value.identity;
            effects.applicationApplied = true;
            PreferenceResult result;
            try {
                result = callback(live);
            }
            catch (const std::exception &error) {
                result = Failure(PreferenceCode::ApplyFailed, live.id, "", error.what());
            }
            catch (...) {
                result = Failure(PreferenceCode::ApplyFailed, live.id, "", "Could not apply preference changes.");
            }
            effects.storageWritten |= result.storageWritten;
            effects.liveWritten |= result.liveWritten;
            effects.applicationApplied |= result.applicationApplied;
            if (!Current(registry, values, isCurrent)) {
                return conflict();
            }
            if (!result.Succeeded()) {
                if (result.pageId.empty())
                    result.pageId = live.id;
                if (result.message.empty())
                    result.message = "Could not apply preference changes.";
                return WithEffects(std::move(result), effects);
            }
        }
        return effects;
    }
} // namespace sky::editor
