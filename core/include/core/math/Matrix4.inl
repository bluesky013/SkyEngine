
namespace sky {

    inline Matrix4::Matrix4() : Matrix4(Vector4(), Vector4(), Vector4(), Vector4())
    {
    }

    inline Matrix4::Matrix4(const Vector4 &r0, const Vector4 &r1, const Vector4 &r2, const Vector4 &r3)
        : m{r0, r1, r2, r3}
    {
    }

    inline const Matrix4 &Matrix4::Identity()
    {
        static Matrix4 matrix;
        matrix.m[0][0] = 1.f;
        matrix.m[1][1] = 1.f;
        matrix.m[2][2] = 1.f;
        matrix.m[3][3] = 1.f;
        return matrix;
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
        const auto& rw = rhs.m;
        Matrix4 ret;
        ret[0] = m[0] * rw[0][0] + m[1] * rw[0][1] + m[2] * rw[0][2] + m[3] * rw[0][3];
        ret[1] = m[0] * rw[1][0] + m[1] * rw[1][1] + m[2] * rw[1][2] + m[3] * rw[1][3];
        ret[2] = m[0] * rw[2][0] + m[1] * rw[2][1] + m[2] * rw[2][2] + m[3] * rw[2][3];
        ret[3] = m[0] * rw[3][0] + m[1] * rw[3][1] + m[2] * rw[3][2] + m[3] * rw[3][3];
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
            m[i] += rhs.m[i];
        }
        return *this;
    }

    inline Matrix4& Matrix4::operator-=(const Matrix4& rhs)
    {
        for (uint32_t i = 0; i < 4; ++i) {
            m[i] -= rhs.m[i];
        }
        return *this;
    }

    inline Matrix4& Matrix4::operator*=(const Matrix4& rhs)
    {
        return *this = (*this) * rhs;
    }

    inline Matrix4& Matrix4::operator*=(float multiplier)
    {
        for (auto & i : m) {
            i *= multiplier;
        }
        return *this;
    }

    inline Matrix4& Matrix4::operator/=(float divisor)
    {
        for (auto & i : m) {
            i /= divisor;
        }
        return *this;
    }

    inline Vector4 Matrix4::operator*(const Vector4& rhs) const
    {
        Vector4 v0 = m[0] * rhs[0];
        Vector4 v1 = m[1] * rhs[1];
        Vector4 v2 = m[2] * rhs[2];
        Vector4 v3 = m[3] * rhs[3];
        return (v0 + v1) + (v2 + v3);
    }

    inline Vector4 &Matrix4::operator[](uint32_t i)
    {
        return m[i];
    }

    inline Vector4 Matrix4::operator[](uint32_t i) const
    {
        return m[i];
    }

    inline void Matrix4::Translate(const Vector3 &rhs)
    {
        m[3] = m[0] * rhs.v[0] + m[1] * rhs.v[1] + m[2] * rhs.v[2] + m[3];
    }

    inline Matrix4 Matrix4::Inverse() const
    {
        float Coef00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
        float Coef02 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
        float Coef03 = m[1][2] * m[2][3] - m[2][2] * m[1][3];

        float Coef04 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
        float Coef06 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
        float Coef07 = m[1][1] * m[2][3] - m[2][1] * m[1][3];

        float Coef08 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
        float Coef10 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
        float Coef11 = m[1][1] * m[2][2] - m[2][1] * m[1][2];

        float Coef12 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
        float Coef14 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
        float Coef15 = m[1][0] * m[2][3] - m[2][0] * m[1][3];

        float Coef16 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
        float Coef18 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
        float Coef19 = m[1][0] * m[2][2] - m[2][0] * m[1][2];

        float Coef20 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
        float Coef22 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
        float Coef23 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

        Vector4 Fac0(Coef00, Coef00, Coef02, Coef03);
        Vector4 Fac1(Coef04, Coef04, Coef06, Coef07);
        Vector4 Fac2(Coef08, Coef08, Coef10, Coef11);
        Vector4 Fac3(Coef12, Coef12, Coef14, Coef15);
        Vector4 Fac4(Coef16, Coef16, Coef18, Coef19);
        Vector4 Fac5(Coef20, Coef20, Coef22, Coef23);

        Vector4 Vec0(m[1][0], m[0][0], m[0][0], m[0][0]);
        Vector4 Vec1(m[1][1], m[0][1], m[0][1], m[0][1]);
        Vector4 Vec2(m[1][2], m[0][2], m[0][2], m[0][2]);
        Vector4 Vec3(m[1][3], m[0][3], m[0][3], m[0][3]);

        Vector4 Inv0(Vec1 * Fac0 - Vec2 * Fac1 + Vec3 * Fac2);
        Vector4 Inv1(Vec0 * Fac0 - Vec2 * Fac3 + Vec3 * Fac4);
        Vector4 Inv2(Vec0 * Fac1 - Vec1 * Fac3 + Vec3 * Fac5);
        Vector4 Inv3(Vec0 * Fac2 - Vec1 * Fac4 + Vec2 * Fac5);

        Vector4 SignA(+1, -1, +1, -1);
        Vector4 SignB(-1, +1, -1, +1);
        Matrix4 Inverse(Inv0 * SignA, Inv1 * SignB, Inv2 * SignA, Inv3 * SignB);

        Vector4 Row0(Inverse[0][0], Inverse[1][0], Inverse[2][0], Inverse[3][0]);

        Vector4 Dot0(m[0] * Row0);
        float Dot1 = (Dot0.x + Dot0.y) + (Dot0.z + Dot0.w);

        float OneOverDeterminant = 1.f / Dot1;
        return Inverse * OneOverDeterminant;
    }

    inline Matrix4 Matrix4::InverseTranspose() const
    {
        float SubFactor00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
        float SubFactor01 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
        float SubFactor02 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
        float SubFactor03 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
        float SubFactor04 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
        float SubFactor05 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
        float SubFactor06 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
        float SubFactor07 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
        float SubFactor08 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
        float SubFactor09 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
        float SubFactor10 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
        float SubFactor11 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
        float SubFactor12 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
        float SubFactor13 = m[1][2] * m[2][3] - m[2][2] * m[1][3];
        float SubFactor14 = m[1][1] * m[2][3] - m[2][1] * m[1][3];
        float SubFactor15 = m[1][1] * m[2][2] - m[2][1] * m[1][2];
        float SubFactor16 = m[1][0] * m[2][3] - m[2][0] * m[1][3];
        float SubFactor17 = m[1][0] * m[2][2] - m[2][0] * m[1][2];
        float SubFactor18 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

        Matrix4 ret;
        ret[0][0] = + (m[1][1] * SubFactor00 - m[1][2] * SubFactor01 + m[1][3] * SubFactor02);
        ret[0][1] = - (m[1][0] * SubFactor00 - m[1][2] * SubFactor03 + m[1][3] * SubFactor04);
        ret[0][2] = + (m[1][0] * SubFactor01 - m[1][1] * SubFactor03 + m[1][3] * SubFactor05);
        ret[0][3] = - (m[1][0] * SubFactor02 - m[1][1] * SubFactor04 + m[1][2] * SubFactor05);

        ret[1][0] = - (m[0][1] * SubFactor00 - m[0][2] * SubFactor01 + m[0][3] * SubFactor02);
        ret[1][1] = + (m[0][0] * SubFactor00 - m[0][2] * SubFactor03 + m[0][3] * SubFactor04);
        ret[1][2] = - (m[0][0] * SubFactor01 - m[0][1] * SubFactor03 + m[0][3] * SubFactor05);
        ret[1][3] = + (m[0][0] * SubFactor02 - m[0][1] * SubFactor04 + m[0][2] * SubFactor05);

        ret[2][0] = + (m[0][1] * SubFactor06 - m[0][2] * SubFactor07 + m[0][3] * SubFactor08);
        ret[2][1] = - (m[0][0] * SubFactor06 - m[0][2] * SubFactor09 + m[0][3] * SubFactor10);
        ret[2][2] = + (m[0][0] * SubFactor11 - m[0][1] * SubFactor09 + m[0][3] * SubFactor12);
        ret[2][3] = - (m[0][0] * SubFactor08 - m[0][1] * SubFactor10 + m[0][2] * SubFactor12);

        ret[3][0] = - (m[0][1] * SubFactor13 - m[0][2] * SubFactor14 + m[0][3] * SubFactor15);
        ret[3][1] = + (m[0][0] * SubFactor13 - m[0][2] * SubFactor16 + m[0][3] * SubFactor17);
        ret[3][2] = - (m[0][0] * SubFactor14 - m[0][1] * SubFactor16 + m[0][3] * SubFactor18);
        ret[3][3] = + (m[0][0] * SubFactor15 - m[0][1] * SubFactor17 + m[0][2] * SubFactor18);

        float det =
            + m[0][0] * ret[0][0]
            + m[0][1] * ret[0][1]
            + m[0][2] * ret[0][2]
            + m[0][3] * ret[0][3];

        ret /= det;

        return ret;
    }
}