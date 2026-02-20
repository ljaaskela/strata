#include "velk_impl.h"

#include <velk_export.h>

namespace velk {

VELK_EXPORT IVelk &instance()
{
    static VelkImpl r;
    return *(r.get_interface<IVelk>());
}

// ---------------------------------------------------------------------------
// Control-block pool
// ---------------------------------------------------------------------------

/**
 * @def VELK_ENABLE_BLOCK_POOL
 * @brief Set to 0 to disable thread-local control_block pooling.
 *
 * Pooling requires thread_local support. Disable on toolchains where
 * thread_local is unavailable or unreliable (e.g. older Android NDK,
 * some embedded targets).
 */
#ifndef VELK_ENABLE_BLOCK_POOL
#define VELK_ENABLE_BLOCK_POOL 1
#endif

#if VELK_ENABLE_BLOCK_POOL

namespace {

constexpr int32_t block_pool_max_size = 256;

/// @brief Thread-local free-list state for control_block recycling.
struct block_pool
{
    control_block* head{nullptr};
    int32_t size{0};

    ~block_pool()
    {
        while (head) {
            auto* next = static_cast<control_block*>(head->ptr);
            delete head;
            head = next;
        }
    }
};

block_pool& get_pool()
{
    thread_local block_pool pool;
    return pool;
}

} // anonymous namespace

VELK_EXPORT control_block* detail::alloc_control_block()
{
    auto& pool = get_pool();
    if (pool.head) {
        auto* b = pool.head;
        pool.head = static_cast<control_block*>(b->ptr);
        --pool.size;
        b->strong.store(1, std::memory_order_relaxed);
        b->weak.store(1, std::memory_order_relaxed);
        b->ptr = nullptr;
        return b;
    }
    return new control_block{1, 1, nullptr};
}

VELK_EXPORT void detail::dealloc_control_block(control_block* block)
{
    auto& pool = get_pool();
    if (pool.size >= block_pool_max_size) {
        delete block;
        return;
    }
    block->ptr = pool.head;
    pool.head = block;
    ++pool.size;
}

#else // !VELK_ENABLE_BLOCK_POOL

VELK_EXPORT control_block* detail::alloc_control_block()
{
    return new control_block{1, 1, nullptr};
}

VELK_EXPORT void detail::dealloc_control_block(control_block* block)
{
    delete block;
}

#endif // VELK_ENABLE_BLOCK_POOL

} // namespace velk
