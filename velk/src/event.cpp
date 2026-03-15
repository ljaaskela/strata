#include "event.h"

#include <velk/api/velk.h>

namespace velk {

EventImpl::~EventImpl()
{
    release_owned_context();
}

void EventImpl::release_owned_context()
{
    if (context_deleter_ && owned_context_) {
        context_deleter_(owned_context_);
    }
    owned_context_ = nullptr;
    context_deleter_ = nullptr;
}

IAny::Ptr EventImpl::callback_trampoline(void* ctx, FnArgs args)
{
    return reinterpret_cast<IFunction::CallableFn*>(ctx)(args);
}

IAny::Ptr EventImpl::invoke(FnArgs args, InvokeType type) const
{
    type = resolve_invoke_type(type, get_object_data().owner_thread_id);
    if (type == Deferred) {
        DeferredTask task;
        task.fn = get_self<IFunction>();
        task.args = ::velk::make_shared<DeferredArgs>(args);
        instance().queue_deferred_tasks(array_view(&task, 1));
        return nullptr;
    }

    IAny::Ptr result;
    if (target_fn_) {
        result = target_fn_(target_context_, args);
    }
    invoke_handlers(args);
    return result;
}

array_view<IFunction::ConstPtr> EventImpl::immediate_handlers() const
{
    return {handlers_.data(), deferred_begin_};
}

array_view<IFunction::ConstPtr> EventImpl::deferred_handlers() const
{
    return {handlers_.data() + deferred_begin_, handlers_.size() - deferred_begin_};
}

void EventImpl::invoke_handlers(FnArgs args) const
{
    // Snapshot handlers: a handler may add/remove handlers on this event
    // during invocation, which would invalidate iterators into handlers_.
    auto snapshot = handlers_;
    auto db = deferred_begin_;

    for (size_t i = 0; i < db; ++i) {
        snapshot[i]->invoke(args);
    }

    if (db >= snapshot.size()) {
        return;
    }
    // Clone args once, share ownership across all deferred tasks
    auto clonedArgs = ::velk::make_shared<DeferredArgs>(args);

    vector<DeferredTask> tasks;
    tasks.reserve(snapshot.size() - db);
    for (size_t i = db; i < snapshot.size(); ++i) {
        tasks.push_back({snapshot[i], clonedArgs});
    }

    // Queue N tasks to instance for execution at next instance().update().
    instance().queue_deferred_tasks(array_view(tasks.data(), tasks.size()));
}

void EventImpl::set_invoke_callback(IFunction::CallableFn* fn)
{
    release_owned_context();
    target_context_ = reinterpret_cast<void*>(fn);
    target_fn_ = fn ? &callback_trampoline : nullptr;
}

void EventImpl::bind(void* context, IFunction::BoundFn* fn)
{
    release_owned_context();
    target_context_ = context;
    target_fn_ = fn;
}

void EventImpl::set_owned_callback(void* context, IFunction::BoundFn* fn, IFunction::ContextDeleter* deleter)
{
    release_owned_context();
    owned_context_ = context;
    context_deleter_ = deleter;
    target_context_ = context;
    target_fn_ = fn;
}

ReturnValue EventImpl::add_handler(const IFunction::ConstPtr& fn, InvokeType type) const
{
    type = resolve_invoke_type(type, get_object_data().owner_thread_id);
    if (!fn) {
        return ReturnValue::InvalidArgument;
    }
    for (const auto& h : handlers_) {
        if (h == fn) {
            return ReturnValue::NothingToDo;
        }
    }
    if (type == Immediate) {
        handlers_.insert(handlers_.begin() + deferred_begin_, fn);
        ++deferred_begin_;
    } else {
        handlers_.push_back(fn);
    }
    return ReturnValue::Success;
}

ReturnValue EventImpl::remove_handler(const IFunction::ConstPtr& fn) const
{
    for (size_t i = 0; i < handlers_.size(); ++i) {
        if (handlers_[i] == fn) {
            if (i < deferred_begin_) {
                --deferred_begin_;
            }
            handlers_.erase(handlers_.begin() + i);
            return ReturnValue::Success;
        }
    }
    return ReturnValue::NothingToDo;
}

bool EventImpl::has_handlers() const
{
    return !handlers_.empty();
}

} // namespace velk
