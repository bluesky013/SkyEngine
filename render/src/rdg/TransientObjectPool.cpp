//
// Created by Zach Lee on 2023/4/16.
//

#include <render/rdg/TransientObjectPool.h>
#include <render/RHI.h>

namespace sky::rdg {
    void TransientObjectPool::ResetPool()
    {
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

    rhi::ImagePtr TransientObjectPool::RequestImage(const rdg::GraphImage &desc)
    {
        auto &list = images[desc];
        for (auto &cacheItem : list) {
            if (!cacheItem.allocated) {
                cacheItem.allocated = true;
                cacheItem.count = 0;
                return cacheItem.item;
            }
        }

        auto image = CreateImageByDesc(desc);
        list.emplace_back(CacheItem<rhi::ImagePtr>{image, 0, true});
        return image;
    }

    rhi::BufferPtr TransientObjectPool::RequestBuffer(const rdg::GraphBuffer &desc)
    {
        auto &list = buffers[desc];
        for (auto &cacheItem : list) {
            if (!cacheItem.allocated) {
                cacheItem.allocated = true;
                cacheItem.count = 0;
                return cacheItem.item;
            }
        }

        auto buffer = CreateBufferByDesc(desc);
        list.emplace_back(CacheItem<rhi::BufferPtr>{buffer, 0, true});
        return buffer;
    }

    void TransientObjectPool::RecycleImage(rhi::ImagePtr &image, const rdg::GraphImage &desc)
    {
        auto &list = images[desc];
        auto iter = std::find_if(list.begin(), list.end(), [&image](const auto &cacheItem) {
            return cacheItem.item.get() == image.get();
        });
        iter->allocated = false;
    }

    void TransientObjectPool::RecycleBuffer(rhi::BufferPtr &buffer, const rdg::GraphBuffer &desc)
    {
        auto &list = buffers[desc];
        auto iter = std::find_if(list.begin(), list.end(), [&buffer](const auto &cacheItem) {
            return cacheItem.item.get() == buffer.get();
        });
        iter->allocated = false;
    }

} // namespace sky::rdg