#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_thread_context.h>
#include <velk/interface/types.h>

#include <shared_mutex>

namespace velk::impl {

/**
 * @brief Default implementation of IThreadContext backed by std::shared_mutex.
 */
class ThreadContext final : public ext::ObjectCore<ThreadContext, IThreadContext>
{
public:
    VELK_CLASS_UID(ClassId::ThreadContext, "ThreadContext");

    void lock_shared() override { mutex_.lock_shared(); }
    void unlock_shared() override { mutex_.unlock_shared(); }
    void lock() override { mutex_.lock(); }
    void unlock() override { mutex_.unlock(); }
    bool try_lock_shared() override { return mutex_.try_lock_shared(); }
    bool try_lock() override { return mutex_.try_lock(); }

private:
    std::shared_mutex mutex_;
};

} // namespace velk::impl

#endif // THREAD_CONTEXT_H
