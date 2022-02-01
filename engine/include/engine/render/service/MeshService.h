//
// Created by Zach Lee on 2022/2/1.
//

#include <engine/render/service/RenderService.h>
#include <core/util/Uuid.h>
#include <engine/asset/MeshAsset.h>
#include <unordered_map>

namespace sky {

    class MeshService : public IService {
    public:
        MeshService();
        ~MeshService();

    private:
        std::unordered_map<Uuid, MeshPtr> meshes;
    };

}