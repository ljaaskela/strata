#ifndef VELK_DURATION_H
#define VELK_DURATION_H

#include <cstdint>

namespace velk {

/** @brief A duration in microseconds. */
struct Duration
{
    int64_t us = 0; ///< Microseconds.

    /** @brief Constructs a Duration from seconds. */
    static constexpr Duration from_seconds(float s) { return {static_cast<int64_t>(s * 1'000'000.f)}; }
    /** @brief Constructs a Duration from milliseconds. */
    static constexpr Duration from_milliseconds(float ms) { return {static_cast<int64_t>(ms * 1'000.f)}; }
    /** @brief Constructs a Duration from microseconds. */
    static constexpr Duration from_microseconds(int64_t us) { return {us}; }

    /** @brief Converts to seconds. */
    constexpr float to_seconds() const { return static_cast<float>(us) / 1'000'000.f; }
    /** @brief Converts to milliseconds. */
    constexpr float to_milliseconds() const { return static_cast<float>(us) / 1'000.f; }
    /** @brief Converts to microseconds. */
    constexpr int64_t to_microseconds() const { return us; }

    /** @brief Returns the sum of two durations. */
    constexpr Duration operator+(Duration rhs) const { return {us + rhs.us}; }
    /** @brief Returns the difference of two durations. */
    constexpr Duration operator-(Duration rhs) const { return {us - rhs.us}; }
    /** @brief Scales a duration by an integer factor. */
    constexpr Duration operator*(int64_t scalar) const { return {us * scalar}; }
    /** @brief Divides a duration by an integer divisor. */
    constexpr Duration operator/(int64_t scalar) const { return {us / scalar}; }
    /** @brief Scales a duration by a floating-point factor. */
    constexpr Duration operator*(float scalar) const { return {static_cast<int64_t>(us * scalar)}; }
    /** @brief Divides a duration by a floating-point divisor. */
    constexpr Duration operator/(float scalar) const { return {static_cast<int64_t>(us / scalar)}; }

    /** @brief Adds another duration to this one. */
    constexpr Duration& operator+=(Duration rhs) { us += rhs.us; return *this; }
    /** @brief Subtracts another duration from this one. */
    constexpr Duration& operator-=(Duration rhs) { us -= rhs.us; return *this; }
    /** @brief Scales this duration by an integer factor. */
    constexpr Duration& operator*=(int64_t scalar) { us *= scalar; return *this; }
    /** @brief Divides this duration by an integer divisor. */
    constexpr Duration& operator/=(int64_t scalar) { us /= scalar; return *this; }

    /** @brief Returns true if two durations are equal. */
    constexpr bool operator==(Duration rhs) const { return us == rhs.us; }
    /** @brief Returns true if two durations are not equal. */
    constexpr bool operator!=(Duration rhs) const { return us != rhs.us; }
    /** @brief Returns true if this duration is shorter than @p rhs. */
    constexpr bool operator<(Duration rhs) const { return us < rhs.us; }
    /** @brief Returns true if this duration is shorter than or equal to @p rhs. */
    constexpr bool operator<=(Duration rhs) const { return us <= rhs.us; }
    /** @brief Returns true if this duration is longer than @p rhs. */
    constexpr bool operator>(Duration rhs) const { return us > rhs.us; }
    /** @brief Returns true if this duration is longer than or equal to @p rhs. */
    constexpr bool operator>=(Duration rhs) const { return us >= rhs.us; }

    /** @brief Returns the negated duration. */
    constexpr Duration operator-() const { return {-us}; }
};

} // namespace velk

#endif // VELK_DURATION_H
