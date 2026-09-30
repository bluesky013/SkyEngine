//
// SlangFileSystemAdapter: bridges an aurora::ShaderFileSystem to Slang's
// ISlangFileSystem so that #include resolves against virtual files (top
// priority) then mounted search paths.
//

#pragma once

struct ISlangFileSystem;

namespace sky::aurora {

    class ShaderFileSystem;

    // Create a Slang ISlangFileSystem over the given ShaderFileSystem.
    // Caller owns the returned interface and must release() it.
    ISlangFileSystem *CreateSlangFileSystem(ShaderFileSystem *fs);

} // namespace sky::aurora
