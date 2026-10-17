#pragma once

#include <cstdint>

namespace sky::aurora {

    enum class GpuErrorCode : uint8_t { NONE, UNSUPPORTED, INVALID_ARGUMENT, OUT_OF_MEMORY, DEVICE_LOST, NATIVE_ERROR };
    struct GpuError {
        GpuErrorCode code       = GpuErrorCode::NONE;
        int64_t      nativeCode = 0;
    };
    struct GpuOperationResult {
        GpuError error;
        bool     Succeeded() const noexcept
        {
            return error.code == GpuErrorCode::NONE;
        }
    };

    enum class QueueSubmitStatus : uint8_t { ACCEPTED, REJECTED, INDETERMINATE };
    struct QueueSubmitResult {
        QueueSubmitStatus status = QueueSubmitStatus::REJECTED;
        GpuError          error;
    };
    enum class FenceStatus : uint8_t { PENDING, SIGNALED, FAILED };
    struct FenceQueryResult {
        FenceStatus status = FenceStatus::FAILED;
        GpuError    error;
    };

} // namespace sky::aurora
