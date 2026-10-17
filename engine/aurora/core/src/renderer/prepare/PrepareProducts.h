#pragma once

#include "scene/DeferredSceneCommands.h"
#include <cstddef>
#include <span>

namespace sky::aurora {

    struct PrepareOutputConfig {
        SystemId system             = 0;
        size_t   maxRecordsPerTask  = 0;
        size_t   maxBytesPerTask    = 0;
        size_t   maxDeferredPerTask = 0;
    };
    struct PrepareOutputRecord {
        PrepareOrigin origin;
        uint64_t      channel = 0;
        size_t        offset  = 0;
        size_t        size    = 0;
    };

    // Owned pointer-free CPU payloads; the producer/consumer share the channel's byte schema.
    struct PrepareProducts {
        std::vector<PrepareOutputRecord> records;
        std::vector<std::byte>           bytes;
        DeferredSceneBatch               deferred;
        std::span<const std::byte>       Read(const PrepareOutputRecord &record) const
        {
            if (record.offset > bytes.size() || record.size > bytes.size() - record.offset)
                return {};
            return std::span<const std::byte>(bytes).subspan(record.offset, record.size);
        }
    };

    struct PrepareTaskProducts {
        PrepareOutputConfig limits;
        PrepareProducts     data;
    };

} // namespace sky::aurora
