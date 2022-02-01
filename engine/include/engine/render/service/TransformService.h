//
// Created by Zach Lee on 2022/2/1.
//

#include <engine/render/service/RenderService.h>

namespace sky {

    class TransformService : public RenderService {
    public:
        TransformService();
        ~TransformService();

        void OnTick(float time) override;

    private:

    };

}