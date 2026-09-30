//
// SlangFileSystemAdapter: bridges ShaderFileSystem to Slang's ISlangFileSystem.
//

#include <aurora/shader/SlangFileSystemAdapter.h>

#include <aurora/shader/ShaderFileSystem.h>

#include <slang.h>

#include <cstring>
#include <string>

namespace sky::aurora {

    namespace {
        bool GuidEquals(const SlangUUID &a, const SlangUUID &b)
        {
            return a.data1 == b.data1 && a.data2 == b.data2 && a.data3 == b.data3 && std::memcmp(a.data4, b.data4, sizeof(a.data4)) == 0;
        }

        class SlangAdapter : public ISlangFileSystem {
        public:
            explicit SlangAdapter(ShaderFileSystem *owner) : mOwner(owner)
            {
            }

            // ISlangUnknown
            SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID &uuid, void **outObject) override
            {
                ISlangUnknown *intf = getInterface(uuid);
                if (intf == nullptr) {
                    return SLANG_E_NO_INTERFACE;
                }
                addRef();
                *outObject = intf;
                return SLANG_OK;
            }

            SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override
            {
                return ++mRefCount;
            }

            SLANG_NO_THROW uint32_t SLANG_MCALL release() override
            {
                if (--mRefCount == 0) {
                    delete this;
                    return 0;
                }
                return mRefCount;
            }

            // ISlangCastable
            SLANG_NO_THROW void *SLANG_MCALL castAs(const SlangUUID &uuid) override
            {
                return getInterface(uuid);
            }

            // ISlangFileSystem
            SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(const char *path, ISlangBlob **outBlob) override
            {
                if (outBlob == nullptr) {
                    return SLANG_E_INVALID_ARG;
                }
                std::string content;
                if (!mOwner->ReadFile(path != nullptr ? path : "", content)) {
                    return SLANG_E_NOT_FOUND;
                }
                *outBlob = slang_createBlob(content.data(), content.size());
                return *outBlob != nullptr ? SLANG_OK : SLANG_FAIL;
            }

        private:
            ISlangUnknown *getInterface(const SlangUUID &uuid)
            {
                if (GuidEquals(uuid, ISlangUnknown::getTypeGuid()) || GuidEquals(uuid, ISlangCastable::getTypeGuid()) ||
                    GuidEquals(uuid, ISlangFileSystem::getTypeGuid())) {
                    return static_cast<ISlangFileSystem *>(this);
                }
                return nullptr;
            }

            ShaderFileSystem *mOwner;
            uint32_t          mRefCount = 1;
        };
    } // namespace

    ISlangFileSystem *CreateSlangFileSystem(ShaderFileSystem *fs)
    {
        return new SlangAdapter(fs);
    }

} // namespace sky::aurora
