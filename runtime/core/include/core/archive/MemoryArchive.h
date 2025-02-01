//
// Created by blues on 2024/1/14.
//

#pragma once

#include <core/concept/Concept.h>
#include <core/archive/IArchive.h>
#include <core/template/ReferenceObject.h>
#include <vector>

namespace sky {

    class MemoryArchive : public IInputArchive, public IOutputArchive, public RefObject {
    public:
        MemoryArchive() = default;
        ~MemoryArchive() override = default;

        bool LoadRaw(char *data, size_t size) override;
        bool SaveRaw(const char *data, size_t size) override;

        const char* Data() const;
        char* Address();
        size_t Size() const;

        void Resize(size_t size);
        void Seek(size_t offset);

    private:
        size_t pointer = 0;
        std::vector<char> data;
    };

} // namespace sky
