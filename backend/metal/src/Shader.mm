//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/Shader.h>
#import <Foundation/NSString.h>
#include <core/logger/Logger.h>
#include <mtl/Device.h>

static const char* TAG = "Metal";

namespace sky::mtl {

    Shader::~Shader()
    {
        if (function) {
            [function release];
            function = nil;
        }
    }

    bool Shader::Init(const Descriptor &desc)
    {
        NSString *nsSource = [NSString stringWithUTF8String: reinterpret_cast<const char*>(desc.data.data())];

        MTLCompileOptions *mtlOptions = [MTLCompileOptions alloc];
        mtlOptions.fastMathEnabled    = YES;
        mtlOptions.languageVersion    = MTLLanguageVersion2_2;

        NSError *error = nil;
        id<MTLLibrary> library = [device.GetMetalDevice() newLibraryWithSource:nsSource
                                                        options:mtlOptions
                                                          error:&error];
        [mtlOptions release];
        if (!library) {
            LOG_E(TAG, "Compile shader failed. %s", [[error localizedDescription] UTF8String]);
            return false;
        }
        function = [library newFunctionWithName:@"main0"];
        [library release];
        if (!function) {
            LOG_E(TAG, "Compile shader create function failed.");
            return false;
        }
        return true;
    }

} // namespace sky::mtl
