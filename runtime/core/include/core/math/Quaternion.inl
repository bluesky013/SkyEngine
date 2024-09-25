

namespace sky {

    inline Quaternion::Quaternion() : Quaternion(1, 0, 0, 0)
    {
    }

    inline Quaternion::Quaternion(float w_, float x_, float y_, float z_) : x(x_), y(y_), z(z_), w(w_)
    {
    }

    inline Quaternion::Quaternion(float angle, const Vector3 &axis)
    {
        float const half = angle * 0.5f;
        float const s = sin(half);
        float const c = cos(half);

        auto tmpAxis = Vector3(axis);
        tmpAxis.Normalize();
        Vector3 tmp = tmpAxis * s;
        x = tmp.x;
        y = tmp.y;
        z = tmp.z;
        w = c;
        tmp.Normalize();
    }

    inline float Quaternion::Dot(const Quaternion &rhs) const
    {
        return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
    }

    inline void Quaternion::Normalize()
    {
        float n = w * w + x * x + y * y + z * z;
        if (n == 0)
        {
            w = 1;
            x = 0;
            y = 0;
            z = 0;
        }
        float inverseSqrt = 1 / sqrt(n);
        Quaternion::operator*=(inverseSqrt);
    }

    inline Quaternion Quaternion::Conjugate() const
    {
        return {w, -x, -y, -z};
    }

    inline Quaternion Quaternion::operator*(const Quaternion &rhs) const
    {
        Quaternion res;
        res.w = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;
        res.x = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
        res.y = w * rhs.y + y * rhs.w + z * rhs.x - x * rhs.z;
        res.z = w * rhs.z + z * rhs.w + x * rhs.y - y * rhs.x;
        return res;
    }

    inline Vector3 Quaternion::operator*(const Vector3 &rhs) const
    {
        Vector3 const tmpVec(x, y, z);
        Vector3 const uv(tmpVec.Cross(rhs));
        Vector3 const uuv(tmpVec.Cross(uv));

        return rhs + ((uv * w) + uuv) * 2.f;
    }

    inline Quaternion &Quaternion::operator*=(float m)
    {
        x *= m;
        y *= m;
        z *= m;
        return *this;
    }

    inline Quaternion &Quaternion::operator/=(float d)
    {
        x /= d;
        y /= d;
        z /= d;
        return *this;
    }

    inline void Quaternion::FromEulerYZX(Vector3 euler)
    {
        float halfToRad = 0.5F * sky::PI / 180.F;
        euler.x *= halfToRad;
        euler.y *= halfToRad;
        euler.z *= halfToRad;
        float sx = std::sin(euler.x);
        float cx = std::cos(euler.x);
        float sy = std::sin(euler.y);
        float cy = std::cos(euler.y);
        float sz = std::sin(euler.z);
        float cz = std::cos(euler.z);

        x = sx * cy * cz + cx * sy * sz;
        y = cx * sy * cz + sx * cy * sz;
        z = cx * cy * sz - sx * sy * cz;
        w = cx * cy * cz - sx * sy * sz;

        Normalize();
    }

    inline Vector3 Quaternion::ToEulerYZX() const
    {
        float bank{0};
        float heading{0};
        float attitude{0};
        float test = x * y + z * w;
        float r2d = 180.F / sky::PI;
        if (test > 0.499999) {
            bank = 0;
            heading = 2 * atan2(x, w) * r2d;
            attitude = 90;
        } else if (test < -0.499999) {
            bank = 0;
            heading = -2 * atan2(x, w) * r2d;
            attitude = -90;
        } else {
            float sqx = x * x;
            float sqy = y * y;
            float sqz = z * z;
            bank = atan2(2 * x * w - 2 * y * z, 1 - 2 * sqx - 2 * sqz) * r2d;
            heading = atan2(2 * y * w - 2 * x * z, 1 - 2 * sqy - 2 * sqz) * r2d;
            attitude = asin(2 * test) * r2d;
        }
        return {bank, heading, attitude};
    }

    inline Matrix4 Quaternion::ToMatrix() const
    {
        Matrix4 res;
        float x2 = x + x;
        float y2 = y + y;
        float z2 = z + z;

        res.m[0][0] = 1 - y2 * y - z2 * z;
        res.m[0][1] = x2 * y + z2 * w;
        res.m[0][2] = x2 * z - y2 * w;
        res.m[0][3] = 0.f;

        res.m[1][0] = x2 * y - z2 * w;
        res.m[1][1] = 1 - x2 * x - z2 * z;
        res.m[1][2] = y2 * z + x2 * w;
        res.m[1][3] = 0.f;

        res.m[2][0] = x2 * z + y2 * w;
        res.m[2][1] = y2 * z - x2 * w;
        res.m[2][2] = 1 - x2 * x - y2 * y;
        res.m[2][3] = 0.f;

        res.m[3][0] = 0.f;
        res.m[3][1] = 0.f;
        res.m[3][2] = 0.f;
        res.m[3][3] = 1.f;

        return res;
    }
}
