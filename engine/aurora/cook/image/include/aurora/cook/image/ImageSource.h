//
// Source image decoding. Stb handles png/jpg/jpeg/hdr; a minimal built-in
// reader handles KTX1/KTX2. A KTX whose payload is already block compressed is
// passed through verbatim (no re-decode) as an aurora ImageAssetData.
//

#pragma once

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/cook/image/ImageProcess.h>

#include <cstdint>
#include <vector>

namespace sky::aurora::cook {

    struct CookImageSource {
        // Set when the source is uncompressed and should enter the normal
        // resize / mip / compress pipeline.
        ImageObjectPtr image;
        // Semantic type for the uncompressed path (KTX may be cube / array / 3D).
        ImageAssetType assetType = ImageAssetType::TEXTURE_2D;
        // Set when the source is already block compressed and can be emitted
        // directly. `asset` carries format / type / dims / slices / rawData.
        ImageAssetData asset;
        bool           precompressed = false;

        bool Valid() const { return image != nullptr || precompressed; }
    };

    // png/jpg/jpeg -> RGBA8, hdr -> RGBA32F. Returns null on failure.
    ImageObjectPtr LoadStbImage(const std::vector<uint8_t> &bytes, bool isHdr);

    // KTX1 / KTX2 (supercompressionScheme == 0). Returns false on parse failure
    // or when a KTX2 supercompressed payload is encountered.
    bool LoadKtx(const std::vector<uint8_t> &bytes, CookImageSource &out);

} // namespace sky::aurora::cook
