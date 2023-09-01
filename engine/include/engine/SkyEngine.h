//
// Created by Zach Lee on 2021/11/12.
//

#pragma once
#include <core/environment/Singleton.h>
#include <framework/window/IWindowEvent.h>
#include <framework/world/World.h>

#include <memory>
#include <set>

namespace sky {

    class SkyEngine : public Singleton<SkyEngine> {
    public:
        static void Reflect();

        bool Init();
        void DeInit();

        void Tick(float);

        void AddWorld(const WorldPtr& world);
        void RemoveWorld(const WorldPtr& world);

    private:
        template <typename T>
        friend class Singleton;

        SkyEngine()  = default;
        ~SkyEngine() override = default;

        std::set<WorldPtr> worlds;
    };

} // namespace sky