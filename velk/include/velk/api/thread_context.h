#ifndef VELK_API_THREAD_CONTEXT_H
#define VELK_API_THREAD_CONTEXT_H

#include <velk/api/velk.h>
#include <velk/interface/intf_thread_context.h>
#include <velk/interface/types.h>

#include <shared_mutex>

namespace velk {

/**
 * @brief Convenience wrapper around IThreadContext::Ptr.
 *
 * Provides read_lock() and write_lock() helpers that return RAII lock guards.
 */
class ThreadContext
{
public:
    ThreadContext() = default;
    explicit ThreadContext(IThreadContext::Ptr ctx) : ctx_(std::move(ctx)) {}

    /** @brief Acquires a shared (reader) lock. Multiple readers can hold this concurrently. */
    std::shared_lock<IThreadContext> read_lock() const { return std::shared_lock(*ctx_); }

    /** @brief Acquires an exclusive (writer) lock. Blocks all other readers and writers. */
    std::unique_lock<IThreadContext> write_lock() const { return std::unique_lock(*ctx_); }

    /** @brief Returns the underlying IThreadContext pointer. */
    IThreadContext::Ptr get() const { return ctx_; }

    /** @brief Implicit conversion to IThreadContext::Ptr. */
    operator IThreadContext::Ptr() noexcept { return ctx_; }
    operator IThreadContext::ConstPtr() const noexcept { return ctx_; }

    /** @brief Returns true if a context is held. */
    explicit operator bool() const noexcept { return ctx_ != nullptr; }

private:
    IThreadContext::Ptr ctx_;
};

/** @brief Creates a new ThreadContext instance. */
inline ThreadContext create_thread_context()
{
    return ThreadContext(instance().create<IThreadContext>(ClassId::ThreadContext));
}

} // namespace velk

#endif // VELK_API_THREAD_CONTEXT_H
