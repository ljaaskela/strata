#ifndef VELK_INTF_THREAD_CONTEXT_H
#define VELK_INTF_THREAD_CONTEXT_H

#include <velk/interface/intf_interface.h>

namespace velk {

/**
 * @brief Opt-in shared reader/writer lock for concurrent property access.
 *
 * Satisfies the C++ SharedMutex named requirements, so std::shared_lock<IThreadContext>
 * and std::unique_lock<IThreadContext> work directly.
 *
 * Locking is purely manual. Users wrap batches of reads or writes with the
 * appropriate lock type. Zero cost for objects that don't use it.
 */
class IThreadContext : public Interface<IThreadContext>
{
public:
    /** @brief Acquires a shared (reader) lock. Multiple threads may hold this concurrently. */
    virtual void lock_shared() = 0;

    /** @brief Releases a shared (reader) lock. */
    virtual void unlock_shared() = 0;

    /** @brief Acquires an exclusive (writer) lock. Blocks until all shared and exclusive locks are released. */
    virtual void lock() = 0;

    /** @brief Releases an exclusive (writer) lock. */
    virtual void unlock() = 0;

    /** @brief Attempts to acquire a shared lock without blocking. @return true if the lock was acquired. */
    virtual bool try_lock_shared() = 0;

    /** @brief Attempts to acquire an exclusive lock without blocking. @return true if the lock was acquired. */
    virtual bool try_lock() = 0;
};

} // namespace velk

#endif // VELK_INTF_THREAD_CONTEXT_H
