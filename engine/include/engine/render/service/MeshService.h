//
// Created by Zach Lee on 2022/2/1.
//

#include <engine/IService.h>
#include <core/util/Uuid.h>
#include <engine/asset/MeshAsset.h>
#include <unordered_map>

namespace sky {

    class MeshService : public IService {
    public:
        MeshService();
        ~MeshService();

        using Handle = SHandle<MeshService>;

    private:
    };

}