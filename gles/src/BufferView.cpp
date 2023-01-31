//
// Created by Zach on 2023/1/31.
//

#include <gles/BufferView.h>
#include <gles/Device.h>

namespace sky::gles {

    bool BufferView::Init(const Descriptor &desc)
    {
        viewDesc = desc;
        return true;
    }

}
