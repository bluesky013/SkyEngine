//
// Created by Zach Lee on 2022/9/25.
//

#include "MacosPlatform.h"
#import <Foundation/Foundation.h>

static const char* TAG = "MacPlatform";

namespace sky {

    bool Platform::Init(const PlatformInfo& info)
    {
        platform = std::make_unique<MacosPlatform>();
        return platform->Init(info);
    }

    std::string MacosPlatform::GetInternalPath() const
    {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentsDirectory = [paths objectAtIndex:0];
        return [documentsDirectory UTF8String];
    }

    std::string MacosPlatform::GetBundlePath() const
    {
        auto *path = [[NSBundle mainBundle] bundlePath];
        return [path UTF8String];
    }
}
