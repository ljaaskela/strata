#ifndef VELK_HASH_H
#define VELK_HASH_H

#include <velk/string_view.h>

#include <cstdint>

namespace velk {

/**
 * @brief Constexpr helper: returns the high 64 bits of a 64x64 multiply.
 */
constexpr uint64_t mulhi64(uint64_t a, uint64_t b)
{
    uint64_t a_lo = a & 0xFFFFFFFF;
    uint64_t a_hi = a >> 32;
    uint64_t b_lo = b & 0xFFFFFFFF;
    uint64_t b_hi = b >> 32;

    uint64_t lo_lo = a_lo * b_lo;
    uint64_t lo_hi = a_lo * b_hi;
    uint64_t hi_lo = a_hi * b_lo;
    uint64_t hi_hi = a_hi * b_hi;

    uint64_t cross = (lo_lo >> 32) + (lo_hi & 0xFFFFFFFF) + (hi_lo & 0xFFFFFFFF);
    return hi_hi + (lo_hi >> 32) + (hi_lo >> 32) + (cross >> 32);
}

/**
 * @brief Plain 128-bit value used as the result of make_hash128.
 *
 * Layout-compatible with Uid (which adds string-parsing constructors and
 * formatting on top). hash.h stays free of Uid-specific concerns so it can
 * be included by code that just wants string hashing without dragging in
 * the type-identification machinery.
 */
struct Hash128
{
    uint64_t hi = 0;
    uint64_t lo = 0;

    constexpr Hash128() = default;
    constexpr Hash128(uint64_t h, uint64_t l) : hi(h), lo(l) {}

    constexpr bool operator==(const Hash128& o) const { return hi == o.hi && lo == o.lo; }
    constexpr bool operator!=(const Hash128& o) const { return !(*this == o); }
};

/**
 * @brief Multiplies a 128-bit value by the FNV-128 prime (2^88 + 315).
 *
 * Exploits the prime's structure: x * prime = (x << 88) + x * 315.
 */
constexpr Hash128 fnv128_multiply(Hash128 x)
{
    // Compute x * 315 as a 128-bit result
    constexpr uint64_t small = 315;
    uint64_t mul_lo = x.lo * small;
    uint64_t mul_hi = mulhi64(x.lo, small) + x.hi * small;

    // Compute x << 88 = (x << 64) << 24. The low 64 bits are always 0.
    // High 64 bits = x.lo << 24 (since x.hi bits shift out entirely for a 128-bit value)
    uint64_t shift_hi = x.lo << 24;

    // Add: (shift_hi, 0) + (mul_hi, mul_lo)
    uint64_t res_lo = mul_lo;
    uint64_t res_hi = mul_hi + shift_hi;

    return {res_hi, res_lo};
}

/**
 * @brief Computes a compile-time FNV-1a 128-bit hash of a string.
 *
 * For type/interface identification, prefer the Uid-returning make_hash
 * wrapper in velk/uid.h, which builds on this function.
 *
 * @param toHash The string to hash.
 * @return The computed 128-bit hash.
 */
constexpr Hash128 make_hash128(const string_view toHash)
{
    // FNV-1a 128-bit: offset basis and prime from the FNV spec
    Hash128 result{0x6c62272e07bb0142, 0x62b821756295c58d};

    for (char c : toHash) {
        result.lo ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        result = fnv128_multiply(result);
    }

    return result;
}

/**
 * @brief Computes a compile-time FNV-1a 64-bit hash of a string.
 *
 * Lightweight string hash suitable for performance-log keys, shader cache
 * lookups, and similar non-identity use cases. For 128-bit type/interface
 * identification, use make_hash (returns Uid) from velk/uid.h.
 *
 * @param toHash The string to hash.
 * @return The computed 64-bit hash.
 */
constexpr uint64_t make_hash64(const string_view toHash)
{
    uint64_t result = 0xcbf29ce484222325ULL;
    for (char c : toHash) {
        result ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        result *= 0x00000100000001B3ULL;
    }
    return result;
}

} // namespace velk

#endif // VELK_HASH_H
