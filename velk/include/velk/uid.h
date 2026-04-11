#ifndef VELK_UID_H
#define VELK_UID_H

#include <velk/hash.h>
#include <velk/string_view.h>

#include <cstdint>
#include <functional>
#include <ostream>

namespace velk {

/** @brief Returns true if @p c is a valid hexadecimal digit. */
constexpr bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/** @brief Returns true if @p str is a valid UUID string (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx). */
constexpr bool is_valid_uid_format(string_view str)
{
    if (str.size() != 36) {
        return false;
    }
    for (size_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (str[i] != '-') {
                return false;
            }
        } else {
            if (!is_hex_digit(str[i])) {
                return false;
            }
        }
    }
    return true;
}

/** @brief Called at runtime for malformed UID strings. Not constexpr, so bad UIDs in
 *  constexpr context produce a compile error. */
inline void uid_format_error() {}

/** @brief 128-bit unique identifier used for type and interface identification. */
struct Uid
{
    uint64_t hi = 0;
    uint64_t lo = 0;

    constexpr Uid() = default;
    constexpr Uid(uint64_t h, uint64_t l) : hi(h), lo(l) {}

    /** @brief Constructs a Uid from a string literal. Validates length at compile time. */
    template <size_t N>
    constexpr Uid(const char (&str)[N]) : Uid(string_view(str, N - 1))
    {
        static_assert(N == 37, "Uid string must be 36 characters (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)");
    }

    /** @brief Constructs a Uid from a UUID string (e.g. "cc262192-d151-941f-d542-d4c622b50b09").
     *  In constexpr context, a malformed string produces a compile error. */
    constexpr Uid(string_view str) : hi(0), lo(0)
    {
        if (!is_valid_uid_format(str)) {
            // Non-constexpr call makes the compiler reject bad UIDs in constexpr context.
            // At runtime, produces a zero Uid.
            uid_format_error();
            return;
        }
        for (auto c : str) {
            if (c == '-') {
                continue;
            }
            uint64_t nibble = (c >= '0' && c <= '9')   ? uint64_t(c - '0')
                              : (c >= 'a' && c <= 'f') ? uint64_t(c - 'a' + 10)
                              : (c >= 'A' && c <= 'F') ? uint64_t(c - 'A' + 10)
                                                       : 0;
            hi = (hi << 4) | (lo >> 60);
            lo = (lo << 4) | nibble;
        }
    }

    constexpr bool operator==(const Uid& o) const { return hi == o.hi && lo == o.lo; }
    constexpr bool operator!=(const Uid& o) const { return !(*this == o); }
    constexpr bool operator<(const Uid& o) const { return hi < o.hi || (hi == o.hi && lo < o.lo); }

    friend std::ostream& operator<<(std::ostream& os, const Uid& u)
    {
        constexpr char hex[] = "0123456789abcdef";
        auto put = [&](uint64_t v, int nibbles) {
            for (int i = (nibbles - 1) * 4; i >= 0; i -= 4) {
                os.put(hex[(v >> i) & 0xF]);
            }
        };
        // GUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        put(u.hi >> 32, 8);
        os.put('-');
        put(u.hi >> 16, 4);
        os.put('-');
        put(u.hi, 4);
        os.put('-');
        put(u.lo >> 48, 4);
        os.put('-');
        put(u.lo, 12);
        return os;
    }
};

static_assert(sizeof(Uid) == 16);

/**
 * @brief Computes a compile-time FNV-1a 128-bit hash of a string, returned
 * as a Uid.
 *
 * Thin wrapper over the generic make_hash128 in velk/hash.h. Use this when
 * the result is consumed as a type or interface identifier.
 *
 * @param toHash The string to hash.
 * @return The computed 128-bit Uid.
 */
constexpr Uid make_hash(const string_view toHash)
{
    Hash128 h = make_hash128(toHash);
    return Uid{h.hi, h.lo};
}

} // namespace velk

template <>
struct std::hash<velk::Uid>
{
    constexpr size_t operator()(const velk::Uid& u) const noexcept
    {
        return static_cast<size_t>(u.hi ^ u.lo);
    }
};

/** @brief Expands a UUID string literal into two uint64_t template arguments (hi, lo). */
#define VELK_UID(str) ::velk::Uid(str).hi, ::velk::Uid(str).lo

#endif // VELK_UID_H
