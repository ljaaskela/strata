#ifndef VELK_API_MATH_TYPES_H
#define VELK_API_MATH_TYPES_H

#include <cmath>
#include <cstdint>

namespace velk {

/// @brief Small float threshold for near-zero comparisons.
constexpr float epsilon = 1e-6f;

/// @brief True if @p x is within @ref epsilon of zero.
constexpr bool is_zero(float x) { return x < epsilon && x > -epsilon; }

/// @brief Convert degrees to radians.
constexpr float deg_to_rad(float deg) { return deg * 0.01745329251994329577f; }

/// @brief Convert radians to degrees.
constexpr float rad_to_deg(float rad) { return rad * 57.2957795130823208768f; }

/// @brief Returns the smaller of @p a and @p b. Ties return @p a.
template <typename T>
constexpr T min(const T& a, const T& b) { return a < b ? a : b; }

/// @brief Returns the larger of @p a and @p b. Ties return @p a.
template <typename T>
constexpr T max(const T& a, const T& b) { return a > b ? a : b; }

/// @brief Clamps @p x to [lo, hi].
template <typename T>
constexpr T clamp(const T& x, const T& lo, const T& hi)
{
    return ::velk::min(::velk::max(x, lo), hi);
}

/// @brief 2D float vector.
struct vec2
{
    float x{}, y{};

    static constexpr vec2 zero() { return {0.f, 0.f}; }
    static constexpr vec2 one() { return {1.f, 1.f}; }
    static constexpr vec2 unit_x() { return {1.f, 0.f}; }
    static constexpr vec2 unit_y() { return {0.f, 1.f}; }

    constexpr vec2 operator+(const vec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
    constexpr vec2 operator-(const vec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
    constexpr vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr vec2 operator/(float s) const { return {x / s, y / s}; }
    constexpr vec2 operator-() const { return {-x, -y}; }
    constexpr vec2& operator+=(const vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
    constexpr vec2& operator-=(const vec2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    constexpr vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    constexpr bool operator==(const vec2& rhs) const { return x == rhs.x && y == rhs.y; }
    constexpr bool operator!=(const vec2& rhs) const { return !(*this == rhs); }
};

constexpr vec2 operator*(float s, const vec2& v) { return v * s; }

/// @brief 3D float vector.
struct vec3
{
    float x{}, y{}, z{};

    static constexpr vec3 zero() { return {0.f, 0.f, 0.f}; }
    static constexpr vec3 one() { return {1.f, 1.f, 1.f}; }
    static constexpr vec3 unit_x() { return {1.f, 0.f, 0.f}; }
    static constexpr vec3 unit_y() { return {0.f, 1.f, 0.f}; }
    static constexpr vec3 unit_z() { return {0.f, 0.f, 1.f}; }

    constexpr vec3 operator+(const vec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    constexpr vec3 operator-(const vec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    constexpr vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    constexpr vec3 operator-() const { return {-x, -y, -z}; }
    constexpr vec3& operator+=(const vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    constexpr vec3& operator-=(const vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    constexpr vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    constexpr bool operator==(const vec3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
    constexpr bool operator!=(const vec3& rhs) const { return !(*this == rhs); }

    static constexpr float dot(const vec3& a, const vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static constexpr vec3 cross(const vec3& a, const vec3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    static constexpr float length_squared(const vec3& v) { return dot(v, v); }

    static float length(const vec3& v) { return std::sqrt(dot(v, v)); }

    /// @brief True if the vector's length is within @ref epsilon of zero.
    static constexpr bool is_zero(const vec3& v) { return length_squared(v) < epsilon * epsilon; }

    static vec3 normalize(const vec3& v)
    {
        float sqr = v.x * v.x + v.y * v.y + v.z * v.z;
        return v * (1.0f / std::sqrt(sqr));
    }
};

constexpr vec3 operator*(float s, const vec3& v) { return v * s; }

/// @brief 4D float vector.
struct vec4
{
    float x{}, y{}, z{}, w{};

    static constexpr vec4 zero() { return {0.f, 0.f, 0.f, 0.f}; }
    static constexpr vec4 one() { return {1.f, 1.f, 1.f, 1.f}; }

    constexpr vec4 operator+(const vec4& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; }
    constexpr vec4 operator-(const vec4& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; }
    constexpr vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    constexpr vec4 operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
    constexpr vec4 operator-() const { return {-x, -y, -z, -w}; }
    constexpr vec4& operator+=(const vec4& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this; }
    constexpr vec4& operator-=(const vec4& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w; return *this; }
    constexpr vec4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
    constexpr bool operator==(const vec4& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w; }
    constexpr bool operator!=(const vec4& rhs) const { return !(*this == rhs); }
};

constexpr vec4 operator*(float s, const vec4& v) { return v * s; }

/// @brief 2D unsigned integer vector.
struct uvec2
{
    uint32_t x{}, y{};

    static constexpr uvec2 zero() { return {0, 0}; }

    constexpr bool operator==(const uvec2& rhs) const { return x == rhs.x && y == rhs.y; }
    constexpr bool operator!=(const uvec2& rhs) const { return !(*this == rhs); }
};

/// @brief 3D dimensions.
struct size
{
    float width{}, height{}, depth{};

    static constexpr size zero() { return {0.f, 0.f, 0.f}; }
    static constexpr size one() { return {1.f, 1.f, 1.f}; }

    constexpr bool operator==(const size& rhs) const { return width == rhs.width && height == rhs.height && depth == rhs.depth; }
    constexpr bool operator!=(const size& rhs) const { return !(*this == rhs); }
};

/// @brief Axis-aligned rectangle (position and dimensions).
struct rect
{
    float x{}, y{}, width{}, height{};

    static constexpr rect zero() { return {0.f, 0.f, 0.f, 0.f}; }

    constexpr bool operator==(const rect& rhs) const { return x == rhs.x && y == rhs.y && width == rhs.width && height == rhs.height; }
    constexpr bool operator!=(const rect& rhs) const { return !(*this == rhs); }
};

/// @brief 3D axis-aligned bounding box (position and extent).
struct aabb
{
    vec3 position{};
    size extent{};

    static constexpr aabb from_size(size s) { return {{}, s}; };
    static constexpr aabb zero() { return {}; }

    /// @brief Empty box marker: inverted min/max so merge(empty, x) == x.
    static constexpr aabb empty()
    {
        aabb a;
        a.position = { 1e30f,  1e30f,  1e30f};
        a.extent   = {-2e30f, -2e30f, -2e30f};
        return a;
    }

    constexpr vec3 min() const { return position; }
    constexpr vec3 max() const { return {position.x + extent.width, position.y + extent.height, position.z + extent.depth}; }

    /// @brief Returns a box covering both inputs. Either may be empty().
    static constexpr aabb merge(const aabb& a, const aabb& b)
    {
        const vec3 a_lo = a.min(), a_hi = a.max();
        const vec3 b_lo = b.min(), b_hi = b.max();
        const vec3 lo{::velk::min(a_lo.x, b_lo.x),
                      ::velk::min(a_lo.y, b_lo.y),
                      ::velk::min(a_lo.z, b_lo.z)};
        const vec3 hi{::velk::max(a_hi.x, b_hi.x),
                      ::velk::max(a_hi.y, b_hi.y),
                      ::velk::max(a_hi.z, b_hi.z)};
        aabb out;
        out.position = lo;
        out.extent = {hi.x - lo.x, hi.y - lo.y, hi.z - lo.z};
        return out;
    }

    /// @brief World-space AABB of this local-space box transformed by
    ///        `m`. Tests all eight corners so rotation/scale produces a
    ///        correct axis-aligned bound; pure translation degenerates
    ///        to a single offset. Defined out-of-line below since it
    ///        depends on @c mat4.
    aabb transformed(const struct mat4& m) const;

    constexpr bool operator==(const aabb& rhs) const { return position == rhs.position && extent == rhs.extent; }
    constexpr bool operator!=(const aabb& rhs) const { return !(*this == rhs); }
};

/// @brief RGBA color. Alpha defaults to 1 (fully opaque).
struct color
{
    float r{}, g{}, b{}, a{1};

    static constexpr color white() { return {1.f, 1.f, 1.f, 1.f}; }
    static constexpr color black() { return {0.f, 0.f, 0.f, 1.f}; }
    static constexpr color transparent() { return {0.f, 0.f, 0.f, 0.f}; }
    static constexpr color red() { return {1.f, 0.f, 0.f, 1.f}; }
    static constexpr color green() { return {0.f, 1.f, 0.f, 1.f}; }
    static constexpr color blue() { return {0.f, 0.f, 1.f, 1.f}; }

    constexpr bool operator==(const color& rhs) const { return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a; }
    constexpr bool operator!=(const color& rhs) const { return !(*this == rhs); }
};

/// @brief 4x4 float matrix, column-major. Defaults to identity.
struct mat4
{
    float m[16]{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    static constexpr mat4 identity() { return {}; }
    static constexpr mat4 zeros() { return {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}; }

    constexpr float& operator()(int row, int col) { return m[col * 4 + row]; }
    constexpr const float& operator()(int row, int col) const { return m[col * 4 + row]; }

    constexpr vec4 col(int c) const { return {m[c*4], m[c*4+1], m[c*4+2], m[c*4+3]}; }
    constexpr vec4 row(int r) const { return {m[r], m[r+4], m[r+8], m[r+12]}; }

    constexpr void set_col(int c, const vec4& v)
    {
        m[c*4] = v.x; m[c*4+1] = v.y; m[c*4+2] = v.z; m[c*4+3] = v.w;
    }

    /// @brief Set the xyz of column @p c, leaving row 3 untouched.
    constexpr void set_col(int c, const vec3& v)
    {
        m[c*4] = v.x; m[c*4+1] = v.y; m[c*4+2] = v.z;
    }

    constexpr void set_row(int r, const vec4& v)
    {
        m[r] = v.x; m[r+4] = v.y; m[r+8] = v.z; m[r+12] = v.w;
    }

    /// @brief Set the xyz of row @p r, leaving column 3 untouched.
    constexpr void set_row(int r, const vec3& v)
    {
        m[r] = v.x; m[r+4] = v.y; m[r+8] = v.z;
    }

    constexpr mat4 operator*(const mat4& rhs) const
    {
        mat4 r = zeros();
        for (int c = 0; c < 4; ++c)
            for (int k = 0; k < 4; ++k)
                for (int row = 0; row < 4; ++row)
                    r.m[c * 4 + row] += m[k * 4 + row] * rhs.m[c * 4 + k];
        return r;
    }

    constexpr vec4 operator*(const vec4& v) const
    {
        return {
            m[0]*v.x + m[4]*v.y + m[ 8]*v.z + m[12]*v.w,
            m[1]*v.x + m[5]*v.y + m[ 9]*v.z + m[13]*v.w,
            m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
            m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
        };
    }

    constexpr bool operator==(const mat4& rhs) const
    {
        for (int i = 0; i < 16; ++i)
            if (m[i] != rhs.m[i]) return false;
        return true;
    }

    constexpr bool operator!=(const mat4& rhs) const { return !(*this == rhs); }

    static constexpr mat4 translate(const vec3& t)
    {
        mat4 r{};
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    static constexpr mat4 scale(const vec3& s)
    {
        mat4 r{};
        r.m[0]  = s.x;
        r.m[5]  = s.y;
        r.m[10] = s.z;
        return r;
    }

    static mat4 rotate_x(float radians)
    {
        float c = std::cos(radians);
        float s = std::sin(radians);
        mat4 r{};
        r.m[5]  = c;  r.m[6]  = s;
        r.m[9]  = -s; r.m[10] = c;
        return r;
    }

    static mat4 rotate_y(float radians)
    {
        float c = std::cos(radians);
        float s = std::sin(radians);
        mat4 r{};
        r.m[0]  = c; r.m[2]  = -s;
        r.m[8]  = s; r.m[10] = c;
        return r;
    }

    static mat4 rotate_z(float radians)
    {
        float c = std::cos(radians);
        float s = std::sin(radians);
        mat4 r{};
        r.m[0] = c;  r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    /// @brief General 4x4 matrix inverse via Cramer's rule.
    /// Returns identity if the matrix is singular (determinant == 0).
    static mat4 inverse(const mat4& m)
    {
        const float* s = m.m;
        float c00 = s[0]*s[5] - s[1]*s[4], c01 = s[0]*s[6] - s[2]*s[4];
        float c02 = s[0]*s[7] - s[3]*s[4], c03 = s[1]*s[6] - s[2]*s[5];
        float c04 = s[1]*s[7] - s[3]*s[5], c05 = s[2]*s[7] - s[3]*s[6];
        float c06 = s[8]*s[13] - s[9]*s[12], c07 = s[8]*s[14] - s[10]*s[12];
        float c08 = s[8]*s[15] - s[11]*s[12], c09 = s[9]*s[14] - s[10]*s[13];
        float c10 = s[9]*s[15] - s[11]*s[13], c11 = s[10]*s[15] - s[11]*s[14];

        float det = c00*c11 - c01*c10 + c02*c09 + c03*c08 - c04*c07 + c05*c06;
        if (det == 0.f) return identity();
        float inv = 1.f / det;

        mat4 r = zeros();
        r.m[0]  = ( s[5]*c11 - s[6]*c10 + s[7]*c09) * inv;
        r.m[1]  = (-s[1]*c11 + s[2]*c10 - s[3]*c09) * inv;
        r.m[2]  = ( s[13]*c05 - s[14]*c04 + s[15]*c03) * inv;
        r.m[3]  = (-s[9]*c05 + s[10]*c04 - s[11]*c03) * inv;
        r.m[4]  = (-s[4]*c11 + s[6]*c08 - s[7]*c07) * inv;
        r.m[5]  = ( s[0]*c11 - s[2]*c08 + s[3]*c07) * inv;
        r.m[6]  = (-s[12]*c05 + s[14]*c02 - s[15]*c01) * inv;
        r.m[7]  = ( s[8]*c05 - s[10]*c02 + s[11]*c01) * inv;
        r.m[8]  = ( s[4]*c10 - s[5]*c08 + s[7]*c06) * inv;
        r.m[9]  = (-s[0]*c10 + s[1]*c08 - s[3]*c06) * inv;
        r.m[10] = ( s[12]*c04 - s[13]*c02 + s[15]*c00) * inv;
        r.m[11] = (-s[8]*c04 + s[9]*c02 - s[11]*c00) * inv;
        r.m[12] = (-s[4]*c09 + s[5]*c07 - s[6]*c06) * inv;
        r.m[13] = ( s[0]*c09 - s[1]*c07 + s[2]*c06) * inv;
        r.m[14] = (-s[12]*c03 + s[13]*c01 - s[14]*c00) * inv;
        r.m[15] = ( s[8]*c03 - s[9]*c01 + s[10]*c00) * inv;
        return r;
    }

    /// @brief Rotation around an arbitrary axis (Rodrigues' formula). Axis must be normalized.
    static mat4 rotate(const vec3& axis, float radians)
    {
        float c = std::cos(radians);
        float s = std::sin(radians);
        float t = 1.f - c;
        float x = axis.x, y = axis.y, z = axis.z;
        mat4 r{};
        r.m[0]  = t*x*x + c;     r.m[1]  = t*x*y + s*z;   r.m[2]  = t*x*z - s*y;
        r.m[4]  = t*x*y - s*z;   r.m[5]  = t*y*y + c;     r.m[6]  = t*y*z + s*x;
        r.m[8]  = t*x*z + s*y;   r.m[9]  = t*y*z - s*x;   r.m[10] = t*z*z + c;
        return r;
    }
};

/// @brief Quaternion (x, y, z, w). Defaults to identity.
struct quat
{
    float x{}, y{}, z{}, w{1};

    static constexpr quat identity() { return {}; }

    static quat from_axis_angle(const vec3& axis, float radians)
    {
        float h = radians * 0.5f;
        float s = std::sin(h);
        return {axis.x * s, axis.y * s, axis.z * s, std::cos(h)};
    }

    constexpr quat operator*(const quat& r) const
    {
        return {
            w*r.x + x*r.w + y*r.z - z*r.y,
            w*r.y - x*r.z + y*r.w + z*r.x,
            w*r.z + x*r.y - y*r.x + z*r.w,
            w*r.w - x*r.x - y*r.y - z*r.z
        };
    }

    constexpr quat conjugate() const { return {-x, -y, -z, w}; }

    static quat normalize(const quat& q)
    {
        float n = 1.f / std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
        return {q.x*n, q.y*n, q.z*n, q.w*n};
    }

    constexpr bool operator==(const quat& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w; }
    constexpr bool operator!=(const quat& rhs) const { return !(*this == rhs); }

    /// @brief Convert to a column-major 4x4 rotation matrix. Quaternion must be normalized.
    constexpr mat4 to_mat4() const
    {
        float xx = x*x, yy = y*y, zz = z*z;
        float xy = x*y, xz = x*z, yz = y*z;
        float wx = w*x, wy = w*y, wz = w*z;
        mat4 r{};
        r.m[0]  = 1.f - 2.f*(yy + zz); r.m[1]  = 2.f*(xy + wz);       r.m[2]  = 2.f*(xz - wy);
        r.m[4]  = 2.f*(xy - wz);       r.m[5]  = 1.f - 2.f*(xx + zz); r.m[6]  = 2.f*(yz + wx);
        r.m[8]  = 2.f*(xz + wy);       r.m[9]  = 2.f*(yz - wx);       r.m[10] = 1.f - 2.f*(xx + yy);
        return r;
    }
};

inline aabb aabb::transformed(const mat4& m) const
{
    const float w = extent.width;
    const float h = extent.height;
    const float d = extent.depth;
    const float cx[8] = {0, w, 0, w, 0, w, 0, w};
    const float cy[8] = {0, 0, h, h, 0, 0, h, h};
    const float cz[8] = {0, 0, 0, 0, d, d, d, d};
    const float ox = position.x, oy = position.y, oz = position.z;

    vec3 lo{ 1e30f,  1e30f,  1e30f};
    vec3 hi{-1e30f, -1e30f, -1e30f};
    for (int i = 0; i < 8; ++i) {
        const float lx = ox + cx[i];
        const float ly = oy + cy[i];
        const float lz = oz + cz[i];
        const float x = m(0,0)*lx + m(0,1)*ly + m(0,2)*lz + m(0,3);
        const float y = m(1,0)*lx + m(1,1)*ly + m(1,2)*lz + m(1,3);
        const float z = m(2,0)*lx + m(2,1)*ly + m(2,2)*lz + m(2,3);
        lo.x = ::velk::min(lo.x, x);
        lo.y = ::velk::min(lo.y, y);
        lo.z = ::velk::min(lo.z, z);
        hi.x = ::velk::max(hi.x, x);
        hi.y = ::velk::max(hi.y, y);
        hi.z = ::velk::max(hi.z, z);
    }
    aabb out;
    out.position = lo;
    out.extent = {hi.x - lo.x, hi.y - lo.y, hi.z - lo.z};
    return out;
}

} // namespace velk

#endif // VELK_API_MATH_TYPES_H
