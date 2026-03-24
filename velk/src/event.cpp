#include "event.h"

#include <velk/api/velk.h>

namespace velk::impl {

Event::~Event()
{
    if (has_standalone_slot()) {
        release_owned_context();
        delete_standalone_slot();
    }
}

void Event::release_owned_context()
{
    auto& c = cb();
    if (c.context_deleter && c.owned_context) {
        c.context_deleter(c.owned_context);
    }
    c.owned_context = nullptr;
    c.context_deleter = nullptr;
}

IAny::Ptr Event::callback_trampoline(void* ctx, FnArgs args)
{
    return reinterpret_cast<IFunction::CallableFn*>(ctx)(args);
}

IAny::Ptr Event::invoke(FnArgs args, InvokeType type) const
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
    auto& c = cb();
    if (c.target_fn) {
        result = c.target_fn(c.target_context, args);
    }
    invoke_handlers(args);
    return result;
}

array_view<IFunction::ConstPtr> Event::immediate_handlers() const
{
    return {handlers_.data(), deferred_begin_};
}

array_view<IFunction::ConstPtr> Event::deferred_handlers() const
{
    return {handlers_.data() + deferred_begin_, handlers_.size() - deferred_begin_};
}

void Event::invoke_handlers(FnArgs args) const
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

void Event::set_invoke_callback(IFunction::CallableFn* fn)
{
    release_owned_context();
    auto& c = cb();
    c.target_context = reinterpret_cast<void*>(fn);
    c.target_fn = fn ? &callback_trampoline : nullptr;
}

void Event::bind(void* context, IFunction::BoundFn* fn)
{
    release_owned_context();
    auto& c = cb();
    c.target_context = context;
    c.target_fn = fn;
}

void Event::set_owned_callback(void* context, IFunction::BoundFn* fn, IFunction::ContextDeleter* deleter)
{
    release_owned_context();
    auto& c = cb();
    c.owned_context = context;
    c.context_deleter = deleter;
    c.target_context = context;
    c.target_fn = fn;
}

ReturnValue Event::add_handler(const IFunction::ConstPtr& fn, InvokeType type) const
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

ReturnValue Event::remove_handler(const IFunction::ConstPtr& fn) const
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

bool Event::has_handlers() const
{
    return !handlers_.empty();
}

} // namespace velk::impl
