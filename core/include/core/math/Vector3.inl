//
// Created by Zach Lee on 2022/9/10.
//

namespace sky {

    Vector3::Vector3() : Vector3(0, 0, 0)
    {
    }

    Vector3::Vector3(float v) : Vector3(v, v, v)
    {
    }

    Vector3::Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_)
    {
    }

    Vector3 Vector3::operator+(const Vector3& rhs) const
    {
        return Vector3(*this) += rhs;
    }

    Vector3 Vector3::operator-() const
    {
        return Vector3(0) - *this;
    }

    Vector3 Vector3::operator-(const Vector3& rhs) const
    {
        return Vector3(*this) -= rhs;
    }

    Vector3 Vector3::operator*(const Vector3& rhs) const
    {
        return Vector3(*this) *= rhs;
    }

    Vector3 Vector3::operator*(float m) const
    {
        return Vector3(*this) * m;
    }

    Vector3 Vector3::operator/(const Vector3& rhs) const
    {
        return Vector3(*this) / rhs;
    }

    Vector3 Vector3::operator/(float d) const
    {
        return Vector3(*this) / d;
    }

    inline Vector3& Vector3::operator+=(const Vector3& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    inline Vector3& Vector3::operator-=(const Vector3& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    inline Vector3& Vector3::operator*=(const Vector3& rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        return *this;
    }

    inline Vector3& Vector3::operator/=(const Vector3& rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        return *this;
    }

    inline Vector3& Vector3::operator*=(float m)
    {
        x *= m;
        y *= m;
        z *= m;
        return *this;
    }

    inline Vector3& Vector3::operator/=(float d)
    {
        x /= d;
        y /= d;
        z /= d;
        return *this;
    }

    inline float &Vector3::operator[](uint32_t i)
    {
        return v[i];
    }

    inline float Vector3::operator[](uint32_t i) const
    {
        return v[i];
    }
}
