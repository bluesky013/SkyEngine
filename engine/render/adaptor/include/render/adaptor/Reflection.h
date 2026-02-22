//
// Created by Zach Lee on 2023/9/3.
//

#pragma once

namespace sky {
    class SerializationContext;

    void ReflectRenderAsset(SerializationContext *context);
    void ReflectRHI(SerializationContext *context);

} // namespace sky