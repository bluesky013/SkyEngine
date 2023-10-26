//
// Created by Zach Lee on 2023/4/16.
//

#include <render/rdg/TransientObjectPool.h>
#include <render/RHI.h>

namespace sky::rdg {
    void TransientObjectPool::ResetPool()
    {
        TransientPool::ResetPool();
        for (auto &[desc, list] : images) {
            auto iter = list.begin();
            for (; iter != list.end();) {
                iter->allocated = false;
                iter->count ++;
                if (iter->count >= 60) {
                    iter = list.erase(iter);
                } else {
                    ++iter;
                }
            }
        }

        for (auto &[desc, list] : buffers) {
            auto iter = list.begin();
            for (; iter != list.end();) {
                iter->allocated = false;
                iter->count ++;
                if (iter->count >= 60) {
                    iter = list.erase(iter);
                } else {
                    ++iter;
                }
            }
        }
    }

    ImageObject *TransientObjectPool::RequestImage(const rdg::GraphImage &desc)
    {
        auto &list = images[desc];
        for (auto &cacheItem : list) {
            if (!cacheItem.allocated) {
                cacheItem.allocated = true;
                cacheItem.count = 0;
                return cacheItem.item.get();
            }
        }

        auto &object = list.emplace_back();
        object.item = std::make_unique<ImageObject>(CreateImageByDesc(desc));
        object.allocated = true;
        object.count = 0;
        return list.back().item.get();
    }

    BufferObject *TransientObjectPool::RequestBuffer(const rdg::GraphBuffer &desc)
    {
        auto &list = buffers[desc];
        for (auto &cacheItem : list) {
            if (!cacheItem.allocated) {
                cacheItem.allocated = true;
                cacheItem.count = 0;
                return cacheItem.item.get();
            }
        }

        auto &object = list.emplace_back();
        object.item = std::make_unique<BufferObject>( CreateBufferByDesc(desc));
        object.allocated = true;
        object.count = 0;
        return list.back().item.get();
    }

    void TransientObjectPool::RecycleImage(rhi::ImagePtr &image, const rdg::GraphImage &desc)
    {
        auto &list = images[desc];
        auto iter = std::find_if(list.begin(), list.end(), [&image](const auto &cacheItem) {
            return cacheItem.item->image.get() == image.get();
        });
        iter->allocated = false;
    }

    void TransientObjectPool::RecycleBuffer(rhi::BufferPtr &buffer, const rdg::GraphBuffer &desc)
    {
        auto &list = buffers[desc];
        auto iter = std::find_if(list.begin(), list.end(), [&buffer](const auto &cacheItem) {
            return cacheItem.item->buffer.get() == buffer.get();
        });
        iter->allocated = false;
    }
} // namespace sky::rdg