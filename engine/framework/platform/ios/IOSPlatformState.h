//
// UIKit-independent lifecycle and framebuffer input state.
//

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace sky::ios {

    class FrameState {
    public:
        bool ConnectScene(uintptr_t identity)
        {
            if (identity == 0 || (scene != 0 && scene != identity)) {
                return false;
            }
            if (scene == 0) {
                scene = identity;
                SetActive(false);
                ++epoch;
            }
            return true;
        }
        bool SetSceneActive(uintptr_t identity, bool value)
        {
            if (identity == 0 || scene != identity) {
                return false;
            }
            SetActive(value);
            return true;
        }
        bool DisconnectScene(uintptr_t identity)
        {
            if (identity == 0 || scene != identity) {
                return false;
            }
            scene = 0;
            SetActive(false);
            SetSurfaceAvailable(false);
            ++epoch;
            return true;
        }
        uintptr_t GetSceneIdentity() const
        {
            return scene;
        }
        void SetActive(bool value)
        {
            if (active != value) {
                active = value;
                ++epoch;
            }
        }
        void SetSurfaceAvailable(bool value)
        {
            if (surfaceAvailable != value) {
                surfaceAvailable = value;
                ++epoch;
            }
        }
        void Start()
        {
            requested = true;
        }
        void Stop()
        {
            requested = false;
        }
        bool CanTick() const
        {
            return active && surfaceAvailable;
        }
        bool ShouldRunLoop() const
        {
            return requested && CanTick();
        }
        uint64_t GetEpoch() const
        {
            return epoch;
        }

    private:
        bool      active           = false;
        bool      surfaceAvailable = false;
        bool      requested        = false;
        uint64_t  epoch            = 0;
        uintptr_t scene            = 0;
    };

    inline double ValidScale(double scale)
    {
        return std::isfinite(scale) && scale > 0.0 ? scale : 1.0;
    }

    inline int32_t ToFramebufferCoordinate(double point, double scale)
    {
        const double pixel = point * ValidScale(scale);
        if (!std::isfinite(pixel)) {
            return 0;
        }
        if (pixel >= std::numeric_limits<int32_t>::max()) {
            return std::numeric_limits<int32_t>::max();
        }
        if (pixel <= std::numeric_limits<int32_t>::min()) {
            return std::numeric_limits<int32_t>::min();
        }
        return static_cast<int32_t>(std::round(pixel));
    }

    inline uint32_t ToFramebufferExtent(double points, double scale)
    {
        const int32_t pixels = ToFramebufferCoordinate(points, scale);
        return pixels > 0 ? static_cast<uint32_t>(pixels) : 0;
    }

    struct PixelPoint {
        int32_t x = 0;
        int32_t y = 0;
    };

    class PrimaryTouch {
    public:
        bool Begin(uintptr_t identity, PixelPoint point)
        {
            if (primary != 0 || identity == 0) {
                return false;
            }
            primary  = identity;
            position = point;
            return true;
        }

        bool Move(uintptr_t identity, PixelPoint point, PixelPoint &delta)
        {
            if (primary == 0 || primary != identity) {
                return false;
            }
            delta    = {point.x - position.x, point.y - position.y};
            position = point;
            return true;
        }

        bool End(uintptr_t identity, PixelPoint point)
        {
            if (primary == 0 || primary != identity) {
                return false;
            }
            position = point;
            primary  = 0;
            return true;
        }

        bool Cancel()
        {
            if (primary == 0) {
                return false;
            }
            primary = 0;
            return true;
        }

        PixelPoint GetPosition() const
        {
            return position;
        }

    private:
        uintptr_t  primary = 0;
        PixelPoint position;
    };

} // namespace sky::ios
