//
// Created by blues on 2024/1/14.
//

#pragma once

#include <core/concept/Concept.h>
#include <core/archive/StreamArchive.h>
#include <sstream>
#include <vector>

namespace sky {

    class MemoryArchive : public StreamArchive {
    public:
        MemoryArchive() : StreamArchive(ss) {}
        ~MemoryArchive() override = default;

        const char* Data() const override { return ss.rdbuf()->view().data(); }
        uint32_t Size() const override { return static_cast<uint32_t>(ss.rdbuf()->view().size()); }
    private:
        std::stringstream ss;
    };

} // namespace sky
