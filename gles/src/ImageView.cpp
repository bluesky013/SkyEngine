//
// Created by Zach on 2023/1/31.
//

#include <gles/ImageView.h>
#include <gles/Device.h>

namespace sky::gles {

    bool ImageView::Init(const Descriptor &desc)
    {
        viewDesc = desc;
        return true;
    }

}
