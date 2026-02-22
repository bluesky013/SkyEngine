//
// Created by blues on 2024/9/11.
//

#include <freetype/freetype.h>
#include <core/environment/Singleton.h>
#include <memory>

namespace sky {

    class FreeTypeLibrary : public Singleton<FreeTypeLibrary> {
    public:
        FreeTypeLibrary() = default;
        ~FreeTypeLibrary() override;

        void Init();

        FT_Library GetLibrary() const { return library; }

    private:
        FT_Library library = nullptr;
    };

} // namespace sky
