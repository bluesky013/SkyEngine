//
// Created by Zach Lee on 2022/9/10.
//

namespace sky {

    inline constexpr Vector2::Vector2() : Vector2(0, 0)
    {
    }

    inline constexpr Vector2::Vector2(float v) : Vector2(v, v)
    {
    }

    inline constexpr Vector2::Vector2(float x_, float y_) : x(x_), y(y_)
    {
    }

    Vector2 Vector2::operator+(const Vector2& rhs) const
    {
        return Vector2(*this) += rhs;
    }

    Vector2 Vector2::operator-() const
    {
        return Vector2(0) - *this;
    }

    Vector2 Vector2::operator-(const Vector2& rhs) const
    {
        return Vector2(*this) -= rhs;
    }

    Vector2 Vector2::operator*(const Vector2& rhs) const
    {
        return Vector2(*this) *= rhs;
    }

    Vector2 Vector2::operator*(float m) const
    {
        return Vector2(*this) *= m;
    }

    Vector2 Vector2::operator/(const Vector2& rhs) const
    {
        return Vector2(*this) /= rhs;
    }

    Vector2 Vector2::operator/(float d) const
    {
        return Vector2(*this) /= d;
    }

    inline Vector2& Vector2::operator+=(const Vector2& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    inline Vector2& Vector2::operator-=(const Vector2& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    inline Vector2& Vector2::operator*=(const Vector2& rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }

    inline Vector2& Vector2::operator/=(const Vector2& rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }

    inline Vector2& Vector2::operator*=(float m)
    {
        x *= m;
        y *= m;
        return *this;
    }

    inline Vector2& Vector2::operator/=(float d)
    {
        x /= d;
        y /= d;
        return *this;
    }

    inline float &Vector2::operator[](uint32_t i)
    {
        return v[i];
    }

    inline float Vector2::operator[](uint32_t i) const
    {
        return v[i];
    }

    inline float Vector2::Dot(const Vector2 &rhs) const
    {
        Vector2 ret = (*this) * rhs;
        return ret.x + ret.y;
    }

    inline float Vector2::Length() const
    {
        return sqrt(Dot(*this));
    }

    inline void Vector2::Normalize()
    {
        float inverseSqrt = 1 / sqrt(Dot(*this));
        Vector2::operator*=(inverseSqrt);
    }
}
