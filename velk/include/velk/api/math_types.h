#ifndef VELK_API_MATH_TYPES_H
#define VELK_API_MATH_TYPES_H

namespace velk {

/// @brief 2D float vector.
struct vec2
{
    float x{}, y{};
};

/// @brief 3D float vector.
struct vec3
{
    float x{}, y{}, z{};
};

/// @brief 4D float vector.
struct vec4
{
    float x{}, y{}, z{}, w{};
};

/// @brief 2D dimensions (width and height).
struct size
{
    float width{}, height{};
};

/// @brief Axis-aligned rectangle (position and dimensions).
struct rect
{
    float x{}, y{}, width{}, height{};
};

/// @brief RGBA color. Alpha defaults to 1 (fully opaque).
struct color
{
    float r{}, g{}, b{}, a{1};
};

} // namespace velk

#endif // VELK_API_MATH_TYPES_H
