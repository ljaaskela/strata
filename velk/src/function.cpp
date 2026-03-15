#include "function.h"

#include <velk/api/velk.h>

namespace velk::impl {

Function::~Function()
{
    release_owned_context();
}

void Function::release_owned_context()
{
    if (context_deleter_ && owned_context_) {
        context_deleter_(owned_context_);
    }
    owned_context_ = nullptr;
    context_deleter_ = nullptr;
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

    if (target_fn_) {
        return target_fn_(target_context_, args);
    }
    return nullptr;
}

void Function::set_invoke_callback(IFunction::CallableFn* fn)
{
    release_owned_context();
    target_context_ = reinterpret_cast<void*>(fn);
    target_fn_ = fn ? &callback_trampoline : nullptr;
}

void Function::bind(void* context, IFunction::BoundFn* fn)
{
    release_owned_context();
    target_context_ = context;
    target_fn_ = fn;
}

void Function::set_owned_callback(void* context, IFunction::BoundFn* fn,
                                      IFunction::ContextDeleter* deleter)
{
    release_owned_context();
    owned_context_ = context;
    context_deleter_ = deleter;
    target_context_ = context;
    target_fn_ = fn;
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
