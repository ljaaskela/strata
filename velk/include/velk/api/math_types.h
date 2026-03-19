#ifndef VELK_API_MATH_TYPES_H
#define VELK_API_MATH_TYPES_H

namespace velk {

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

    static constexpr aabb zero() { return {}; }

    constexpr vec3 min() const { return position; }
    constexpr vec3 max() const { return {position.x + extent.width, position.y + extent.height, position.z + extent.depth}; }

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
};

} // namespace velk

#endif // VELK_API_MATH_TYPES_H
