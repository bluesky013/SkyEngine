//
// Created by Zach Lee on 2022/6/22.
//

#pragma once

#include <core/environment/Singleton.h>
#include <unordered_map>
#include <set>

namespace sky {

    struct EventTraits {
        using KeyType   = void;
        using MutexType = void;
    };

    template <typename Interface, class KeyType = typename Interface::KeyType>
    class Event {
    public:
        Event()  = default;
        ~Event() = default;

        class Storage : public Singleton<Storage> {
        public:
            void Emplace(const KeyType &key, Interface *listener)
            {
                auto &set = listeners[key];
                set.emplace_back(listener);
            }

            void Erase(Interface *listener)
            {
                for (auto &pair : listeners) {
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
                auto iter = listeners.find(key);
                if (iter == listeners.end()) {
                    return;
                }

                for (auto &listener : iter->second) {
                    std::invoke(func, listener, std::forward<Args>(args)...);
                }
            }

        private:
            friend class Singleton<Storage>;
            Storage()  = default;
            ~Storage() = default;
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
            void Emplace(Interface *listener)
            {
                listeners.emplace(listener);
            }

            void Erase(Interface *listener)
            {
                auto iter = listeners.find(listener);
                if (iter != listeners.end()) {
                    listeners.erase(iter);
                }
            }

            template <typename T, typename... Args>
            void BroadCast(T &&func, Args &&...args)
            {
                for (auto &listener : listeners) {
                    std::invoke(func, listener, std::forward<Args>(args)...);
                }
            }

        private:
            friend class Singleton<Storage>;
            Storage()  = default;
            ~Storage() = default;
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

    template <typename T, typename KeyType = typename T::KeyType>
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
