//
// Created by blues on 2026/3/16.
//

#pragma once

#include <cstdint>
#include <array>
#include <numeric>
#include <algorithm>
#include <cmath>

namespace sky {

    class PerlinNoise {
    public:
        explicit PerlinNoise(uint32_t seed = 0)
        {
            std::iota(perm.begin(), perm.end(), static_cast<uint8_t>(0));

            uint32_t s = seed;
            for (int i = 255; i > 0; --i) {
                s = s * 1664525u + 1013904223u;
                int j = static_cast<int>((s >> 16) % (i + 1));
                std::swap(perm[i], perm[j]);
            }
        }

        double Octave2D_01(double x, double y, int octaves) const
        {
            double result = 0.0;
            double amp = 1.0;
            double freq = 1.0;
            double maxAmp = 0.0;

            for (int i = 0; i < octaves; ++i) {
                result += Noise2D(x * freq, y * freq) * amp;
                maxAmp += amp;
                amp *= 0.5;
                freq *= 2.0;
            }
            return (result / maxAmp) * 0.5 + 0.5;
        }

        double Octave3D_01(double x, double y, double z, int octaves) const
        {
            double result = 0.0;
            double amp = 1.0;
            double freq = 1.0;
            double maxAmp = 0.0;

            for (int i = 0; i < octaves; ++i) {
                result += Noise3D(x * freq, y * freq, z * freq) * amp;
                maxAmp += amp;
                amp *= 0.5;
                freq *= 2.0;
            }
            return (result / maxAmp) * 0.5 + 0.5;
        }

    private:
        std::array<uint8_t, 256> perm{};

        uint8_t Hash(int x) const { return perm[static_cast<uint8_t>(x)]; }

        static double Fade(double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
        static double Lerp(double a, double b, double t) { return a + t * (b - a); }

        static double Grad2D(int hash, double x, double y)
        {
            switch (hash & 3) {
                case 0: return  x + y;
                case 1: return -x + y;
                case 2: return  x - y;
                case 3: return -x - y;
            }
            return 0.0;
        }

        static double Grad3D(int hash, double x, double y, double z)
        {
            int h = hash & 15;
            double u = h < 8 ? x : y;
            double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
            return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
        }

        double Noise2D(double x, double y) const
        {
            int xi = static_cast<int>(std::floor(x)) & 255;
            int yi = static_cast<int>(std::floor(y)) & 255;
            double xf = x - std::floor(x);
            double yf = y - std::floor(y);

            double u = Fade(xf);
            double v = Fade(yf);

            int aa = Hash(Hash(xi    ) + yi    );
            int ab = Hash(Hash(xi    ) + yi + 1);
            int ba = Hash(Hash(xi + 1) + yi    );
            int bb = Hash(Hash(xi + 1) + yi + 1);

            return Lerp(
                Lerp(Grad2D(aa, xf,     yf    ), Grad2D(ba, xf - 1, yf    ), u),
                Lerp(Grad2D(ab, xf,     yf - 1), Grad2D(bb, xf - 1, yf - 1), u),
                v);
        }

        double Noise3D(double x, double y, double z) const
        {
            int xi = static_cast<int>(std::floor(x)) & 255;
            int yi = static_cast<int>(std::floor(y)) & 255;
            int zi = static_cast<int>(std::floor(z)) & 255;
            double xf = x - std::floor(x);
            double yf = y - std::floor(y);
            double zf = z - std::floor(z);

            double u = Fade(xf);
            double v = Fade(yf);
            double w = Fade(zf);

            int aaa = Hash(Hash(Hash(xi    ) + yi    ) + zi    );
            int aab = Hash(Hash(Hash(xi    ) + yi    ) + zi + 1);
            int aba = Hash(Hash(Hash(xi    ) + yi + 1) + zi    );
            int abb = Hash(Hash(Hash(xi    ) + yi + 1) + zi + 1);
            int baa = Hash(Hash(Hash(xi + 1) + yi    ) + zi    );
            int bab = Hash(Hash(Hash(xi + 1) + yi    ) + zi + 1);
            int bba = Hash(Hash(Hash(xi + 1) + yi + 1) + zi    );
            int bbb = Hash(Hash(Hash(xi + 1) + yi + 1) + zi + 1);

            return Lerp(
                Lerp(
                    Lerp(Grad3D(aaa, xf,     yf,     zf    ), Grad3D(baa, xf - 1, yf,     zf    ), u),
                    Lerp(Grad3D(aba, xf,     yf - 1, zf    ), Grad3D(bba, xf - 1, yf - 1, zf    ), u),
                    v),
                Lerp(
                    Lerp(Grad3D(aab, xf,     yf,     zf - 1), Grad3D(bab, xf - 1, yf,     zf - 1), u),
                    Lerp(Grad3D(abb, xf,     yf - 1, zf - 1), Grad3D(bbb, xf - 1, yf - 1, zf - 1), u),
                    v),
                w);
        }
    };

} // namespace sky
