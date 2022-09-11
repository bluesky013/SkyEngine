
namespace sky {

    Matrix4::Matrix4() : Matrix4(Vector4(), Vector4(), Vector4(), Vector4())
    {
    }

    Matrix4::Matrix4(const Vector4 &r0, const Vector4 &r1, const Vector4 &r2, const Vector4 &r3)
        : cols{r0, r1, r2, r3}
    {
    }

    inline Matrix4 Matrix4::operator+(const Matrix4& rhs) const
    {
        return Matrix4(*this) += rhs;
    }

    inline Matrix4 Matrix4::operator-(const Matrix4& rhs) const
    {
        return Matrix4(*this) -= rhs;
    }

    inline Matrix4 Matrix4::operator*(const Matrix4& rhs) const
    {
        auto& rw = rhs.cols;
        Matrix4 ret;
        ret[0] = cols[0] * rw[0][0] + cols[1] * rw[0][1] + cols[2] * rw[0][2] + cols[3] * rw[0][3];
        ret[1] = cols[0] * rw[1][0] + cols[1] * rw[1][1] + cols[2] * rw[1][2] + cols[3] * rw[1][3];
        ret[2] = cols[0] * rw[2][0] + cols[1] * rw[2][1] + cols[2] * rw[2][2] + cols[3] * rw[2][3];
        ret[3] = cols[0] * rw[3][0] + cols[1] * rw[3][1] + cols[2] * rw[3][2] + cols[3] * rw[3][3];
        return ret;
    }

    inline Matrix4 Matrix4::operator*(float multiplier) const
    {
        return Matrix4(*this) *= multiplier;
    }

    inline Matrix4 Matrix4::operator/(float divisor) const
    {
        return Matrix4(*this) /= divisor;
    }

    inline Matrix4 Matrix4::operator-() const
    {
        return Matrix4() - (*this);
    }

    inline Matrix4& Matrix4::operator+=(const Matrix4& rhs)
    {
        for (uint32_t i = 0; i < 4; ++i) {
            cols[i] += rhs.cols[i];
        }
        return *this;
    }

    inline Matrix4& Matrix4::operator-=(const Matrix4& rhs)
    {
        for (uint32_t i = 0; i < 4; ++i) {
            cols[i] -= rhs.cols[i];
        }
        return *this;
    }

    inline Matrix4& Matrix4::operator*=(const Matrix4& rhs)
    {
        return *this = (*this) * rhs;
    }

    inline Matrix4& Matrix4::operator*=(float multiplier)
    {
        for (uint32_t i = 0; i < 4; ++i) {
            cols[i] *= multiplier;
        }
        return *this;
    }

    inline Matrix4& Matrix4::operator/=(float divisor)
    {
        for (uint32_t i = 0; i < 4; ++i) {
            cols[i] /= divisor;
        }
        return *this;
    }

    inline Vector4 Matrix4::operator*(const Vector4& rhs) const
    {
//        return {{ rows[0].v[0] * vector.v[0] + rows[0].v[1] * vector.v[1] + rows[0].v[2] * vector.v[2] + rows[0].v[3] * vector.v[3]
//                    , rows[1].v[0] * vector.v[0] + rows[1].v[1] * vector.v[1] + rows[1].v[2] * vector.v[2] + rows[1].v[3] * vector.v[3]
//                    , rows[2].v[0] * vector.v[0] + rows[2].v[1] * vector.v[1] + rows[2].v[2] * vector.v[2] + rows[2].v[3] * vector.v[3]
//                    , rows[3].v[0] * vector.v[0] + rows[3].v[1] * vector.v[1] + rows[3].v[2] * vector.v[2] + rows[3].v[3] * vector.v[3] }};
    }

    inline Vector4 &Matrix4::operator[](uint32_t i)
    {
        return cols[i];
    }

    inline Vector4 Matrix4::operator[](uint32_t i) const
    {
        return cols[i];
    }
}