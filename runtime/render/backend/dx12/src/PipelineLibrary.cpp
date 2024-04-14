//
// Created by blues on 2023/10/13.
//

#include <dx12/PipelineLibrary.h>
#include <dx12/Device.h>

namespace sky::dx {

    bool PipelineLibrary::Init(const Descriptor &desc)
    {
        ComPtr<ID3DBlob> blob;
        return SUCCEEDED(device.GetDevice()->CreatePipelineLibrary(blob.GetAddressOf(), desc.dataSize, IID_PPV_ARGS(library.GetAddressOf())));
    }

} // namespace sky::dx