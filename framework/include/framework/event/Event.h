//
// Created by Zach Lee on 2022/6/22.
//

#pragma once

#include "core/environment/Singleton.h"
#include <set>
#include <map>

namespace sky {

    struct EventTrait {
        using KeyType = void;
        using MutexType = void;
    };

    template <typename Interface, class KeyType = typename Interface::KeyType>
    class Event {
    public:
        Event() = default;
        ~Event() = default;

        class Storage : public Singleton<Storage> {
        public:
            void Emplace(const KeyType& key, Interface* listener)
            {
                auto& set = listeners[key];
                set.emplace(listener);
            }

            void Erase(Interface* listener)
            {
                for (auto& pair : listeners) {
                    auto iter = pair.second.find(listener);
                    if (iter != pair.second.end()) {
                        pair.second.erase(iter);
                        break;
                    }
                }
            }

            template <typename T, typename ...Args>
            void BroadCast(const KeyType& key, T&& func, Args&& ...args)
            {
                auto iter = listeners.find(key);
                if (iter == listeners.end()) {
                    return;
                }

                for (auto& listener : iter->second) {
                    std::invoke(func, listener, std::forward<Args>(args)...);
                }
            }

        private:
            friend class Singleton<Storage>;
            Storage() = default;
            ~Storage() = default;
            std::map<KeyType, std::set<Interface*>> listeners;
        };

        static void Connect(const KeyType& key, Interface* listener)
        {
            Storage::Get()->Emplace(key, listener);
        }

        static void DisConnect(Interface* listener)
        {
            Storage::Get()->Erase(listener);
        }

        template <typename T, typename ...Args>
        static void BroadCast(const KeyType& key, T&& func, Args&& ...args)
        {
            Storage::Get()->BroadCast(key, std::forward<T>(func), std::forward<Args>(args)...);
        }
    };

    template <typename Interface>
    class Event<Interface, void> {
    public:
        Event() = default;
        ~Event() = default;

        class Storage : public Singleton<Storage> {
        public:
            void Emplace(Interface* listener)
            {
                listeners.emplace(listener);
            }

            void Erase(Interface* listener)
            {
                auto iter = listeners.find(listener);
                if (iter != listeners.end()) {
                    listeners.erase(iter);
                }
            }

            template <typename T, typename ...Args>
            void BroadCast(T&& func, Args&& ...args)
            {
                for (auto& listener : listeners) {
                    std::invoke(func, listener, std::forward<Args>(args)...);
                }
            }

        private:
            friend class Singleton<Storage>;
            Storage() = default;
            ~Storage() = default;
            std::set<Interface*> listeners;
        };

        static void Connect(Interface* listener)
        {
            Storage::Get()->Emplace(listener);
        }

        static void DisConnect(Interface* listener)
        {
            Storage::Get()->Erase(listener);
        }

        template <typename T, typename ...Args>
        static void BroadCast(T&& func, Args&& ...args)
        {
            Storage::Get()->BroadCast(std::forward<T>(func), std::forward<Args>(args)...);
        }
    };
}