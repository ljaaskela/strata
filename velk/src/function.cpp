#include "function.h"

#include <velk/api/velk.h>

namespace velk::impl {

Function::~Function()
{
    if (has_standalone_slot()) {
        release_owned_context();
        delete_standalone_slot();
    }
}

void Function::release_owned_context()
{
    auto& c = cb();
    if (c.context_deleter && c.owned_context) {
        c.context_deleter(c.owned_context);
    }
    c.owned_context = nullptr;
    c.context_deleter = nullptr;
}

IAny::Ptr Function::callback_trampoline(void* ctx, FnArgs args)
{
    return reinterpret_cast<IFunction::CallableFn*>(ctx)(args);
}

IAny::Ptr Function::invoke(FnArgs args, InvokeType type) const
{
    type = resolve_invoke_type(type, get_object_data().owner_thread_id);
    if (type == Deferred) {
        DeferredTask task;
        task.fn = get_self<IFunction>();
        task.args = ::velk::make_shared<DeferredArgs>(args);
        instance().queue_deferred_tasks(array_view(&task, 1));
        return nullptr;
    }

    auto& c = cb();
    if (c.target_fn) {
        return c.target_fn(c.target_context, args);
    }
    return nullptr;
}

void Function::set_invoke_callback(IFunction::CallableFn* fn)
{
    release_owned_context();
    auto& c = cb();
    c.target_context = reinterpret_cast<void*>(fn);
    c.target_fn = fn ? &callback_trampoline : nullptr;
}

void Function::bind(void* context, IFunction::BoundFn* fn)
{
    release_owned_context();
    auto& c = cb();
    c.target_context = context;
    c.target_fn = fn;
}

void Function::set_owned_callback(void* context, IFunction::BoundFn* fn,
                                      IFunction::ContextDeleter* deleter)
{
    release_owned_context();
    auto& c = cb();
    c.owned_context = context;
    c.context_deleter = deleter;
    c.target_context = context;
    c.target_fn = fn;
}

ReturnValue Function::add_handler(const IFunction::ConstPtr& /*fn*/, InvokeType /*type*/) const
{
    return ReturnValue::NothingToDo;
}

ReturnValue Function::remove_handler(const IFunction::ConstPtr& /*fn*/) const
{
    return ReturnValue::NothingToDo;
}

bool Function::has_handlers() const
{
    return false;
}

} // namespace velk::impl
