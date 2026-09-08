//
// Aurora RDG execute-lambda context.
//

#pragma once

#include <core/name/Name.h>
#include <core/memory/TransientAllocator.h>

#include <aurora/rdg/RDGHandles.h>

#include <string_view>
#include <vector>

namespace sky::aurora {

    class Image;
    class Buffer;
    class CommandBuffer;

    class RDGContext {
    public:
        RDGContext()  = default;
        ~RDGContext() = default;

        Image  *GetTexture(RDGTextureHandle handle) const
        {
            if (!handle.IsValid() || mImageTable == nullptr) {
                return nullptr;
            }
            return (*mImageTable)[handle.id];
        }

        Buffer *GetBuffer(RDGBufferHandle handle) const
        {
            if (!handle.IsValid() || mBufferTable == nullptr) {
                return nullptr;
            }
            return (*mBufferTable)[handle.id];
        }

        std::string_view GetPassName() const { return mPassName.GetStr(); }
        CommandBuffer   *GetCommandBuffer() const { return mCommandBuffer; }

        // internal wiring
        void SetCommandBuffer(CommandBuffer *cmdBuf) { mCommandBuffer = cmdBuf; }
        void SetPassName(const Name &name) { mPassName = name; }
        void SetImageTable(const TransientVector<Image *> *table) { mImageTable = table; }
        void SetBufferTable(const TransientVector<Buffer *> *table) { mBufferTable = table; }

    private:
        CommandBuffer              *mCommandBuffer = nullptr;
        Name                        mPassName;
        const TransientVector<Image *> *mImageTable    = nullptr;
        const TransientVector<Buffer *> *mBufferTable  = nullptr;
    };

} // namespace sky::aurora
