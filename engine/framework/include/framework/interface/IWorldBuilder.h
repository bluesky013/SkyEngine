//
// Created by Zach Lee on 2022/3/12.
//

#pragma once

#include <string>
#include <list>
#include <core/template/ReferenceObject.h>
#include <core/event/Event.h>
#include <framework/world/World.h>

namespace sky {
    class IWorldBuilder : public RefObject {
    public:
        IWorldBuilder() = default;
        ~IWorldBuilder() override = default;

        virtual void Build(const WorldPtr &) const = 0;
        virtual std::string GetDesc() const = 0;
    };

    class IWorldBuilderGather : public EventTraits {
    public:
        IWorldBuilderGather() = default;
        ~IWorldBuilderGather() = default;

        virtual void Gather(std::list<CounterPtr<IWorldBuilder>> &builders) const = 0;
    };

} // namespace sky