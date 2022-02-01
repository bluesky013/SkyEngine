//
// Created by Zach Lee on 2022/2/1.
//

#include <engine/IService.h>

namespace sky {

    class TransformService : public IService
    {
    public:
        TransformService();
        ~TransformService();

        void OnTick(float time) override;

    private:

    };

}