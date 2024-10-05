//
// Created by blues on 2024/10/5.
//

#pragma once

#include <core/type/Rtti.h>

namespace sky {

    class SequenceVisitor {
    public:
        SequenceVisitor() = default;
        SequenceVisitor(const ContainerInfo* containerInfo, void* ptr) : info(containerInfo), object(ptr) {}

        SequenceVisitor(const SequenceVisitor&) = default;
        SequenceVisitor &operator=(const SequenceVisitor&) = default;

        SequenceVisitor(SequenceVisitor &&) noexcept = default;
        SequenceVisitor &operator=(SequenceVisitor &&) noexcept = default;

        void* Emplace();
        void Erase(size_t index);

        void* GetByIndex(size_t index);
        size_t Count() const;

        const Uuid &GetValueType() const;

    private:
        const ContainerInfo* info = nullptr;
        void* object = nullptr;
    };


} // namespace sky
