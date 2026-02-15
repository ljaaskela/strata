#include "function.h"
#include <api/strata.h>

namespace strata {

ReturnValue FunctionImpl::callback_trampoline(void* ctx, FnArgs args)
{
    return reinterpret_cast<IFunction::CallableFn*>(ctx)(args);
}

ReturnValue FunctionImpl::invoke(FnArgs args, InvokeType type) const
{
    if (type == Deferred) {
        IStrata::DeferredTask task;
        task.fn = get_self<IFunction>();
        for (size_t i = 0; i < args.count; ++i) {
            task.args.push_back(args[i] ? args[i]->clone() : nullptr);
        }
        instance().queue_deferred_tasks(array_view(&task, 1));
        return ReturnValue::SUCCESS;
    }

    ReturnValue result = ReturnValue::NOTHING_TO_DO;
    if (target_fn_) {
        result = target_fn_(target_context_, args);
    }
    invoke_handlers(args);

    if (target_fn_) {
        return result;
    }
    return handlers_.empty() ? ReturnValue::NOTHING_TO_DO : ReturnValue::SUCCESS;
}

array_view<IFunction::ConstPtr> FunctionImpl::immediate_handlers() const
{
    return {handlers_.data(), deferred_begin_};
}

array_view<IFunction::ConstPtr> FunctionImpl::deferred_handlers() const
{
    return {handlers_.data() + deferred_begin_, handlers_.size() - deferred_begin_};
}

void FunctionImpl::invoke_handlers(FnArgs args) const
{
    for (const auto &h : immediate_handlers()) {
        h->invoke(args);
    }
    if (auto deferred = deferred_handlers(); !deferred.empty()) {
        // Clone each arg for deferred tasks
        std::vector<IAny::Ptr> clonedArgs;
        std::vector<const IAny*> ptrs;
        clonedArgs.reserve(args.count);
        ptrs.reserve(args.count);
        for (size_t i = 0; i < args.count; ++i) {
            clonedArgs.push_back(args[i] ? args[i]->clone() : nullptr);
            ptrs.push_back(clonedArgs.back().get());
        }
        FnArgs clonedFnArgs{ptrs.data(), ptrs.size()};

        std::vector<IStrata::DeferredTask> tasks;
        tasks.reserve(deferred.size());
        for (const auto &h : deferred) {
            // Share the same cloned args across all deferred tasks
            tasks.push_back({h, clonedArgs});
        }
        instance().queue_deferred_tasks(array_view(tasks.data(), tasks.size()));
    }
}

void FunctionImpl::set_invoke_callback(IFunction::CallableFn *fn)
{
    target_context_ = reinterpret_cast<void*>(fn);
    target_fn_ = fn ? &callback_trampoline : nullptr;
}

void FunctionImpl::bind(void* context, IFunctionInternal::BoundFn* fn)
{
    target_context_ = context;
    target_fn_ = fn;
}

ReturnValue FunctionImpl::add_handler(const IFunction::ConstPtr &fn, InvokeType type) const
{
    if (!fn) {
        return ReturnValue::INVALID_ARGUMENT;
    }
    for (const auto &h : handlers_) {
        if (h == fn) return ReturnValue::NOTHING_TO_DO;
    }
    if (type == Immediate) {
        handlers_.insert(handlers_.begin() + deferred_begin_, fn);
        ++deferred_begin_;
    } else {
        handlers_.push_back(fn);
    }
    return ReturnValue::SUCCESS;
}

ReturnValue FunctionImpl::remove_handler(const IFunction::ConstPtr &fn) const
{
    for (size_t i = 0; i < handlers_.size(); ++i) {
        if (handlers_[i] == fn) {
            if (i < deferred_begin_) --deferred_begin_;
            handlers_.erase(handlers_.begin() + i);
            return ReturnValue::SUCCESS;
        }
    }
    return ReturnValue::NOTHING_TO_DO;
}

bool FunctionImpl::has_handlers() const
{
    return !handlers_.empty();
}

} // namespace strata
