#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_thread_context.h>
#include <velk/interface/types.h>

#include <shared_mutex>

namespace velk {

/**
 * @brief Default implementation of IThreadContext backed by std::shared_mutex.
 */
class ThreadContextImpl final : public ext::ObjectCore<ThreadContextImpl, IThreadContext>
{
public:
    VELK_CLASS_UID(ClassId::ThreadContext);

    void lock_shared() override { mutex_.lock_shared(); }
    void unlock_shared() override { mutex_.unlock_shared(); }
    void lock() override { mutex_.lock(); }
    void unlock() override { mutex_.unlock(); }
    bool try_lock_shared() override { return mutex_.try_lock_shared(); }
    bool try_lock() override { return mutex_.try_lock(); }

private:
    std::shared_mutex mutex_;
};

} // namespace velk

#endif // THREAD_CONTEXT_H
