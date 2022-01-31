//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>

namespace sky {

    class BufferAsset : public AssetBase {
    public:
        BufferAsset(const Uuid& id) : AssetBase(id) {}
        ~BufferAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("1ba844f8-3032-45de-8f6a-0010dd9f2656");

        template<class Archive>
        void load(Archive& ar)
        {
        }

        template<class Archive>
        void save(Archive& ar) const
        {
        }

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

    class Buffer : public ResourceBase {
    public:
        Buffer(const Uuid& id) : ResourceBase(id) {}
        ~Buffer() = default;
    };
    using BufferPtr = CounterPtr<Buffer>;

}