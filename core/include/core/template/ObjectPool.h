//
// Created by Zach Lee on 2022/11/22.
//

#pragma once

namespace sky {

    template <typename T>
    class ObjectPool {
    public:
        ObjectPool(uint32_t npp = 16) : numberPerStorage(npp), currentNumber(0), capacity(0) {}
        ~ObjectPool()
        {
            for (auto &storage : storages) {
                delete []storage.data;
            }
        }
        template <typename ...Args>
        T* Allocate(Args &&...args)
        {
            if (!freeList.empty()) {
                T *back = freeList.back();
                new (back) T(std::forward<Args>(args)...);
                freeList.pop_back();
                return back;
            }
            if (currentNumber == capacity) {
                AllocateStorage();
            }
            uint32_t storageIndex = currentNumber / numberPerStorage;
            uint32_t storageOffset = currentNumber % numberPerStorage;
            ++currentNumber;
            T* data = reinterpret_cast<T*>(storages[storageIndex].data + storageOffset * sizeof(T));
            new (data) T(std::forward<Args>(args)...);
            return data;
        }

        void Free(T *data)
        {
            data->~T();
            freeList.emplace_back(data);
        }

    private:
        struct Storage {
            uint8_t *data = nullptr;
        };

        void AllocateStorage()
        {
            auto &back = storages.emplace_back();
            back.data = new uint8_t[numberPerStorage * sizeof(T)];
            capacity = static_cast<uint32_t>(storages.size() * numberPerStorage);
        }

        uint32_t numberPerStorage;
        uint32_t currentNumber;
        uint32_t capacity;
        std::vector<Storage> storages;
        std::vector<T*> freeList;
    };
}
