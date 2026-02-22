//
// Created by blues on 2024/9/11.
//
#include <framework/interface/IModule.h>
#include <framework/compression/Compressor.h>
#include <compression/LZ4.h>

namespace sky {

    class CompressionModule : public IModule {
    public:
        CompressionModule() = default;
        ~CompressionModule() override = default;

        void Start() override
        {
            CompressionManager::Get()->Register(CompressionMethod::LZ4, new LZ4Compressor());
        }

        void Shutdown() override
        {
            CompressionManager::Get()->UnRegister(CompressionMethod::LZ4);
        }
    };
} // namespace sky
REGISTER_MODULE(sky::CompressionModule)