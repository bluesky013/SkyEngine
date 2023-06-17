//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>

namespace sky::dx {

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;
}