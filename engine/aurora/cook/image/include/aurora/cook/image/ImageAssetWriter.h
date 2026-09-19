//
// Maps cooked images into the aurora ImageAssetData payload (slice table +
// raw data) consumed by CreateTextureFromAsset.
//

#pragma once

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/cook/image/ImageProcess.h>

namespace sky::aurora::cook {

    // Uncompressed image (2D / cube / array / 3D) -> asset data. Cube / array
    // emit one slice per (mip, layer); 3D emits one slice per mip volume.
    void WriteImageAsset(const ImageObject &image, ImageAssetType type, ImageAssetData &out);

    // Single-layer compressed image (BC / ASTC) -> asset data, one slice per mip.
    void WriteImageAsset(const CompressedImage &image, ImageAssetType type, ImageAssetData &out);

} // namespace sky::aurora::cook
