//
// Created by Zach Lee on 2022/6/22.
//

#pragma once

#include <core/environment/Singleton.h>
#include <unordered_map>
#include <set>
#include <vector>
#include <algorithm>
#include <functional>
#include <mutex>
#include <type_traits>

namespace sky {

    enum StorageMode : uint8_t {
        IMMEDIATE,
        BATCHED
    };

    struct EventTraits {
        using KeyType   = void;
        using MutexType = void;

        static constexpr StorageMode STORAGE = StorageMode::IMMEDIATE;
    };

    // No-op lockable used when an event opts out of locking (EventTraits::MutexType == void).
    struct NullEventMutex {
        void lock() {}
        void unlock() {}
    };

    template <typename M>
    using EventMutex = std::conditional_t<std::is_void_v<M>, NullEventMutex, M>;

    template <typename Interface, class KeyType = Interface::KeyType>
    class Event {
    public:
        Event()  = default;
        ~Event() = default;

        class Storage : public Singleton<Storage> {
        public:
            using MutexType = EventMutex<typename Interface::MutexType>;

            void Emplace(const KeyType &key, Interface *listener)
            {
                std::lock_guard<MutexType> lock(mutex);
                auto &set = this->listeners[key];
                set.emplace_back(listener);
            }

            void Erase(Interface *listener)
            {
                std::lock_guard<MutexType> lock(mutex);
                for (auto &pair : this->listeners) {
                    auto iter = std::find(pair.second.begin(), pair.second.end(), listener);
                    if (iter != pair.second.end()) {
                        pair.second.erase(iter);
                        break;
                    }
                }
            }

            template <typename T, typename... Args>
            void BroadCast(const KeyType &key, T &&func, Args &&...args)
            {
                // Snapshot under lock, invoke outside so a listener may connect/disconnect safely.
                std::vector<Interface *> snapshot;
                {
                    std::lock_guard<MutexType> lock(mutex);
                    auto iter = this->listeners.find(key);
                    if (iter == this->listeners.end()) {
                        return;
                    }
                    snapshot = iter->second;
                }

                for (auto *listener : snapshot) {
                    std::invoke(func, listener, std::forward<Args>(args)...);
                }
            }

        private:
            friend class Singleton<Storage>;
            Storage()  = default;
            ~Storage() override = default;
            mutable MutexType mutex;
            std::unordered_map<KeyType, std::vector<Interface *>> listeners;
        };

        static void Connect(const KeyType &key, Interface *listener)
        {
            Storage::Get()->Emplace(key, listener);
        }

        static void DisConnect(Interface *listener)
        {
            Storage::Get()->Erase(listener);
        }

        template <typename T, typename... Args>
        static void BroadCast(const KeyType &key, T &&func, Args &&...args)
        {
            Storage::Get()->BroadCast(key, std::forward<T>(func), std::forward<Args>(args)...);
        }
    };

    template <typename Interface>
    class Event<Interface, void> {
    public:
        Event()  = default;
        ~Event() = default;

        class Storage : public Singleton<Storage> {
        public:
            using MutexType = EventMutex<typename Interface::MutexType>;

            void Emplace(Interface *listener)
            {
                std::lock_guard<MutexType> lock(mutex);
                listeners.emplace(listener);
            }

            void Erase(Interface *listener)
            {
                std::lock_guard<MutexType> lock(mutex);
                auto iter = listeners.find(listener);
                if (iter != listeners.end()) {
                    listeners.erase(iter);
                }
            }

            template <typename T, typename... Args>
            void BroadCast(T &&func, Args &&...args)
            {
                std::vector<Interface *> snapshot;
                {
                    std::lock_guard<MutexType> lock(mutex);
                    snapshot.assign(listeners.begin(), listeners.end());
                }

                for (auto *listener : snapshot) {
                    std::invoke(func, listener, std::forward<Args>(args)...);
                }
            }

        private:
            friend class Singleton<Storage>;
            Storage()  = default;
            ~Storage() = default;
            mutable MutexType mutex;
            std::set<Interface *> listeners;
        };

        static void Connect(Interface *listener)
        {
            Storage::Get()->Emplace(listener);
        }

        static void DisConnect(Interface *listener)
        {
            Storage::Get()->Erase(listener);
        }

        template <typename T, typename... Args>
        static void BroadCast(T &&func, Args &&...args)
        {
            Storage::Get()->BroadCast(std::forward<T>(func), std::forward<Args>(args)...);
        }
    };

    template <typename T, typename KeyType = T::KeyType>
    class EventBinder {
    public:
        EventBinder() = default;
        ~EventBinder()
        {
            Reset();
        }

        void Bind(T *inter, KeyType key)
        {
            Event<T>::Connect(key, inter);
            val = inter;
        }

        void Reset()
        {
            if (val != nullptr) {
                Event<T>::DisConnect(val);
                val = nullptr;
            }
        }

    private:
        T *val = nullptr;
    };

    template <typename T>
    class EventBinder<T, void> {
    public:
        EventBinder() = default;
        ~EventBinder()
        {
            Reset();
        }

        void Bind(T *inter)
        {
            Event<T>::Connect(inter);
            val = inter;
        }

        void Reset()
        {
            if (val != nullptr) {
                Event<T>::DisConnect(val);
                val = nullptr;
            }
        }

    private:
        T *val = nullptr;
    };
} // namespace sky
