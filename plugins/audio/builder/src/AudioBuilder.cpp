//
// Created on 2026/09/21.
//

#include <builder/audio/AudioBuilder.h>

#include <audio/AudioClipAsset.h>
#include <audio/AudioImport.h>
#include <framework/asset/AssetManager.h>

#include <core/logger/Logger.h>

#include <miniaudio.h>

static const char *TAG = "AudioBuilder";

namespace sky::builder {

    std::string_view AudioBuilder::QueryType(const std::string &ext) const
    {
        return AssetTraits<AudioClip>::ASSET_TYPE;
    }

    void AudioBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        result.error.clear();
        if (!request.assetInfo || !request.file) {
            result.error = "Audio cook requires an identified source file";
            return;
        }
        std::vector<uint8_t> raw;
        request.file->ReadBin(raw);

        if (raw.empty()) {
            LOG_W(TAG, "Empty audio source: %s", request.assetInfo->path.GetStr().c_str());
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        auto      *manager = AssetManager::Get();
        const auto asset   = std::static_pointer_cast<Asset<AudioClip>>(
            manager->CreateBuildAsset(request.assetInfo->uuid, Name(AssetTraits<AudioClip>::ASSET_TYPE.data())));
        if (!asset) {
            result.error = "Audio asset handler is unavailable";
            return;
        }
        auto &data  = asset->Data();

        data.desc.source     = request.assetInfo->path.GetStr();
        data.desc.loadMode   = AudioLoadMode::InMemory;
        data.desc.defaultBus = audio::SFX_BUS;
        data.desc.loop       = false;

        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        ma_decoder        decoder;
        if (ma_decoder_init_memory(raw.data(), raw.size(), &config, &decoder) != MA_SUCCESS) {
            LOG_W(TAG, "Unsupported audio source: %s", data.desc.source.c_str());
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        ma_uint64 length = 0;
        ma_decoder_get_length_in_pcm_frames(&decoder, &length);
        data.desc.sampleRate = decoder.outputSampleRate;
        data.desc.channels   = decoder.outputChannels;
        if (decoder.outputSampleRate != 0) {
            data.desc.duration = static_cast<float>(length) / static_cast<float>(decoder.outputSampleRate);
        }
        ma_decoder_uninit(&decoder);

        data.rawData.storage = std::move(raw);

        if (const auto saved = manager->SaveAsset(asset, request.bundle.empty() ? request.target : request.bundle); !saved) {
            result.error = saved.message;
            return;
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::builder
