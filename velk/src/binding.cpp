#include "binding.h"

#include "function.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_any.h>

namespace velk {

namespace {

// Thread-local loop detection for binding evaluation and on_changed propagation.
// Only touched inside BindingImpl; zero cost for unbound properties.
static constexpr size_t kMaxBindingDepth = 64;
static thread_local const BindingImpl* tls_evaluating[kMaxBindingDepth] = {};
static thread_local size_t tls_eval_depth = 0;
static thread_local const BindingImpl* tls_notifying[kMaxBindingDepth] = {};
static thread_local size_t tls_notify_depth = 0;

struct LoopGuard
{
    size_t& depth;
    bool looped = false;

    LoopGuard(const BindingImpl* s, const BindingImpl* (&arr)[kMaxBindingDepth], size_t& d) : depth(d)
    {
        for (size_t i = 0; i < depth; ++i) {
            if (arr[i] == s) {
                looped = true;
                return;
            }
        }
        if (depth < kMaxBindingDepth) {
            arr[depth++] = s;
        } else {
            looped = true;
        }
    }

    ~LoopGuard()
    {
        if (!looped && depth > 0) {
            --depth;
        }
    }
};

} // namespace

BindingImpl::~BindingImpl()
{
    unsubscribe();
}

// IBinding

IProperty::ConstPtr BindingImpl::get_source_property() const
{
    return source_property_;
}

IFunction::ConstPtr BindingImpl::get_source_function() const
{
    return source_function_;
}

// IBindingInternal

void BindingImpl::set_source_property(const IProperty::ConstPtr& source)
{
    unsubscribe();
    source_property_ = source;
    source_function_ = {};
    deps_.clear();
    cache_valid_ = false;
    if (inner_) {
        subscribe();
    }
}

void BindingImpl::set_source_function(const IFunction::ConstPtr& fn, vector<IProperty::ConstPtr> deps)
{
    unsubscribe();
    source_property_ = {};
    source_function_ = fn;
    deps_ = std::move(deps);
    cache_valid_ = false;
    if (inner_) {
        subscribe();
    }
}

void BindingImpl::set_invoke_type(InvokeType type)
{
    invoke_type_ = type;
}

// IAnyExtension

bool BindingImpl::set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner)
{
    // Type compatibility check: if both inner (target) and source have values,
    // they must be compatible types.
    auto source = evaluate();
    if (inner && source && !is_compatible(inner, source)) {
        return false;
    }
    inner_ = std::move(inner);
    owner_ = owner;
    subscribe();
    return true;
}

IAny::Ptr BindingImpl::take_inner(IInterface& owner)
{
    // Copy the current bound value into the inner so the property retains
    // the last value it had while bound.
    if (inner_) {
        auto source = evaluate();
        if (source) {
            inner_->copy_from(*source);
        }
    }
    unsubscribe();
    return std::move(inner_);
}

// IAny overrides

array_view<Uid> BindingImpl::get_compatible_types() const
{
    auto source = evaluate();
    return source ? source->get_compatible_types()
                  : (inner_ ? inner_->get_compatible_types() : array_view<Uid>{});
}

size_t BindingImpl::get_data_size(Uid type) const
{
    auto source = evaluate();
    return source ? source->get_data_size(type) : (inner_ ? inner_->get_data_size(type) : 0);
}

ReturnValue BindingImpl::get_data(void* to, size_t toSize, Uid type) const
{
    LoopGuard guard(this, tls_evaluating, tls_eval_depth);
    if (guard.looped) {
        return inner_ ? inner_->get_data(to, toSize, type) : ReturnValue::Fail;
    }

    auto source = evaluate();
    if (source) {
        return source->get_data(to, toSize, type);
    }
    return inner_ ? inner_->get_data(to, toSize, type) : ReturnValue::Fail;
}

ReturnValue BindingImpl::set_data(void const*, size_t, Uid)
{
    return ReturnValue::Fail;
}

ReturnValue BindingImpl::copy_from(const IAny&)
{
    return ReturnValue::Fail;
}

IAny::Ptr BindingImpl::clone() const
{
    auto source = evaluate();
    return source ? source->clone() : (inner_ ? inner_->clone() : nullptr);
}

// Private

IAny::ConstPtr BindingImpl::evaluate() const
{
    if (source_property_) {
        return source_property_->get_value();
    }
    if (source_function_) {
        if (cache_valid_) {
            return cached_result_;
        }
        vector<IAny::ConstPtr> dep_values;
        vector<const IAny*> dep_ptrs;
        dep_values.reserve(deps_.size());
        dep_ptrs.reserve(deps_.size());
        for (auto& dep : deps_) {
            auto val = dep ? dep->get_value() : nullptr;
            dep_ptrs.push_back(val.get());
            dep_values.push_back(std::move(val));
        }
        FnArgs args{dep_ptrs.data(), dep_ptrs.size()};
        cached_result_ = source_function_->invoke(args);
        cache_valid_ = true;
        return cached_result_;
    }
    return nullptr;
}

void BindingImpl::subscribe()
{
    if (subscribed_) {
        return;
    }

    // Create handler via the type registry, then bind a trampoline to this.
    auto fn = instance().create<IFunction>(ClassId::Function);
    if (auto* internal = interface_cast<IFunctionInternal>(fn)) {
        auto* trampoline = +[](void* ctx, FnArgs) -> IAny::Ptr {
            static_cast<BindingImpl*>(ctx)->on_source_changed();
            return nullptr;
        };
        internal->bind(this, trampoline);
    }
    handler_ = fn;

    if (source_property_) {
        if (auto evt = source_property_->on_changed()) {
            evt->add_handler(handler_);
        }
    }
    for (auto& dep : deps_) {
        if (dep) {
            if (auto evt = dep->on_changed()) {
                evt->add_handler(handler_);
            }
        }
    }
    subscribed_ = true;
}

void BindingImpl::unsubscribe()
{
    if (!subscribed_ || !handler_) {
        return;
    }

    if (source_property_) {
        if (auto evt = source_property_->on_changed()) {
            evt->remove_handler(handler_);
        }
    }
    for (auto& dep : deps_) {
        if (dep) {
            if (auto evt = dep->on_changed()) {
                evt->remove_handler(handler_);
            }
        }
    }
    subscribed_ = false;
    handler_ = {};
}

void BindingImpl::on_source_changed()
{
    // Loop detection for on_changed propagation (prevents infinite recursion
    // when A is bound to B and B is bound to A).
    LoopGuard guard(this, tls_notifying, tls_notify_depth);
    if (guard.looped) {
        return;
    }

    cache_valid_ = false;

    auto owner = owner_.lock();
    if (!owner) {
        return;
    }

    if (invoke_type_ == Deferred) {
        // Queue a notification-only deferred property set (null value).
        // The binding evaluates lazily on the next read; this just ensures
        // on_changed fires once during update().
        auto propWeak = IPropertyInternal::WeakPtr(interface_pointer_cast<IPropertyInternal>(owner));
        instance().queue_deferred_property({std::move(propWeak), nullptr});
        return;
    }

    auto* prop = interface_cast<IProperty>(owner);
    if (prop) {
        auto self = get_self<IAnyExtension>();
        const IAny* arg = interface_cast<IAny>(self);
        invoke_event(prop->on_changed(), arg);
    }
}

} // namespace velk
