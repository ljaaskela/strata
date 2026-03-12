#include "binding.h"

#include "dependency_tracker.h"
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

// Property-to-property binding data. Holds a weak ref to the source property.
struct PropertyBindingData final : BindingData
{
    IProperty::ConstWeakPtr source;

    explicit PropertyBindingData(const IProperty::ConstPtr& src) : source(src) {}

    IAny::ConstPtr evaluate() const override
    {
        if (auto s = source.lock()) {
            return s->get_value();
        }
        return nullptr;
    }

    void subscribe(const IFunction::Ptr& handler) override
    {
        if (auto s = source.lock()) {
            if (auto evt = s->on_changed()) {
                evt->add_handler(handler);
            }
        }
    }

    void unsubscribe(const IFunction::Ptr& handler) override
    {
        if (auto s = source.lock()) {
            if (auto evt = s->on_changed()) {
                evt->remove_handler(handler);
            }
        }
    }

    IProperty::ConstPtr get_source_property() const override { return source.lock(); }
};

// Helper: subscribe/unsubscribe a handler to/from a list of dependency properties.
void subscribe_deps(const vector<IProperty::ConstWeakPtr>& deps, const IFunction::Ptr& handler)
{
    for (auto& dep : deps) {
        if (auto locked = dep.lock()) {
            if (auto evt = locked->on_changed()) {
                evt->add_handler(handler);
            }
        }
    }
}

void unsubscribe_deps(const vector<IProperty::ConstWeakPtr>& deps, const IFunction::Ptr& handler)
{
    for (auto& dep : deps) {
        if (auto locked = dep.lock()) {
            if (auto evt = locked->on_changed()) {
                evt->remove_handler(handler);
            }
        }
    }
}

// Function binding data with explicit dependencies.
struct FunctionBindingDataManual final : BindingData
{
    IFunction::ConstPtr function;
    vector<IProperty::ConstWeakPtr> deps;
    mutable IAny::Ptr cached_result;
    mutable bool cache_valid = false;

    FunctionBindingDataManual(const IFunction::ConstPtr& fn, vector<IProperty::ConstPtr> dep_list)
        : function(fn)
    {
        deps.reserve(dep_list.size());
        for (auto& d : dep_list) {
            deps.emplace_back(d);
        }
    }

    IAny::ConstPtr evaluate() const override
    {
        if (cache_valid) {
            return cached_result;
        }

        vector<IAny::ConstPtr> dep_values;
        vector<const IAny*> dep_ptrs;
        dep_values.reserve(deps.size());
        dep_ptrs.reserve(deps.size());
        for (auto& dep : deps) {
            if (auto locked = dep.lock()) {
                auto val = locked->get_value();
                dep_ptrs.push_back(val.get());
                dep_values.push_back(std::move(val));
            } else {
                dep_ptrs.push_back(nullptr);
            }
        }
        FnArgs args{dep_ptrs.data(), dep_ptrs.size()};
        cached_result = function->invoke(args);
        cache_valid = true;
        return cached_result;
    }

    void subscribe(const IFunction::Ptr& handler) override { subscribe_deps(deps, handler); }
    void unsubscribe(const IFunction::Ptr& handler) override { unsubscribe_deps(deps, handler); }
    IFunction::ConstPtr get_source_function() const override { return function; }
    void invalidate() override { cache_valid = false; }
};

// Function binding data with auto-detected dependencies.
struct FunctionBindingDataAuto final : BindingData
{
    IFunction::ConstPtr function;
    vector<IProperty::ConstWeakPtr> deps;
    mutable IAny::Ptr cached_result;
    mutable bool cache_valid = false;
    IFunction::Ptr active_handler;

    explicit FunctionBindingDataAuto(const IFunction::ConstPtr& fn) : function(fn) {}

    IAny::ConstPtr evaluate() const override
    {
        if (cache_valid) {
            return cached_result;
        }

        detail::TrackerScope scope;

        cached_result = function->invoke({});
        cache_valid = true;

        auto& tracked = scope.tracker.deps;

        // Check if deps changed (different size or different pointers)
        bool changed = (tracked.size() != deps.size());
        if (!changed) {
            for (size_t i = 0; i < tracked.size(); ++i) {
                if (tracked[i] != deps[i].unsafe_get()) {
                    changed = true;
                    break;
                }
            }
        }

        if (changed) {
            auto* self = const_cast<FunctionBindingDataAuto*>(this);
            if (self->active_handler) {
                unsubscribe_deps(self->deps, self->active_handler);
            }
            self->deps = scope.tracker.acquire();
            if (self->active_handler) {
                subscribe_deps(self->deps, self->active_handler);
            }
        }

        return cached_result;
    }

    void subscribe(const IFunction::Ptr& handler) override
    {
        active_handler = handler;
        subscribe_deps(deps, handler);
    }

    void unsubscribe(const IFunction::Ptr& handler) override
    {
        unsubscribe_deps(deps, handler);
        active_handler = {};
    }

    IFunction::ConstPtr get_source_function() const override { return function; }
    void invalidate() override { cache_valid = false; }
};

} // namespace

// BindingImpl

BindingImpl::~BindingImpl()
{
    unsubscribe();
}

// IBinding

IProperty::ConstPtr BindingImpl::get_source_property() const
{
    return data_ ? data_->get_source_property() : nullptr;
}

IFunction::ConstPtr BindingImpl::get_source_function() const
{
    return data_ ? data_->get_source_function() : nullptr;
}

bool BindingImpl::add_target(const IProperty::Ptr& target)
{
    auto* pi = interface_cast<IPropertyInternal>(target.get());
    return pi && pi->install_extension(get_self<IAnyExtension>());
}

bool BindingImpl::remove_target(const IProperty::Ptr& target)
{
    auto* pi = interface_cast<IPropertyInternal>(target.get());
    return pi && pi->remove_extension(get_self<IAnyExtension>());
}

void BindingImpl::uninstall()
{
    // Iterate a copy since remove_extension calls take_inner which modifies targets_.
    auto targets = targets_;
    for (auto& entry : targets) {
        auto owner = entry.owner.lock();
        if (!owner) {
            continue;
        }
        auto* pi = interface_cast<IPropertyInternal>(owner);
        if (pi) {
            pi->remove_extension(get_self<IAnyExtension>());
        }
    }
}

// IBindingInternal

void BindingImpl::set_source_property(const IProperty::ConstPtr& source)
{
    unsubscribe();
    data_ = std::make_unique<PropertyBindingData>(source);
    if (!targets_.empty()) {
        subscribe();
    }
}

void BindingImpl::set_source_function(const IFunction::ConstPtr& fn, vector<IProperty::ConstPtr> deps)
{
    unsubscribe();
    data_ = std::make_unique<FunctionBindingDataManual>(fn, std::move(deps));
    if (!targets_.empty()) {
        subscribe();
    }
}

void BindingImpl::set_source_function(const IFunction::ConstPtr& fn)
{
    unsubscribe();
    data_ = std::make_unique<FunctionBindingDataAuto>(fn);
    // Don't subscribe yet; deps are discovered on first evaluate().
}

void BindingImpl::set_invoke_type(InvokeType type)
{
    invoke_type_ = type;
}

// IAnyExtension

IAny::ConstPtr BindingImpl::get_inner() const
{
    return !targets_.empty() ? IAny::ConstPtr(targets_[0].inner) : nullptr;
}

bool BindingImpl::set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner)
{
    auto source = evaluate();
    if (inner && source && !is_compatible(inner, source)) {
        return false;
    }
    bool was_empty = targets_.empty();
    targets_.push_back({owner, std::move(inner)});
    if (was_empty && data_) {
        subscribe();
    }
    return true;
}

IAny::Ptr BindingImpl::take_inner(IInterface& owner)
{
    for (size_t i = 0; i < targets_.size(); ++i) {
        auto locked = targets_[i].owner.lock();
        if (locked.get() == &owner) {
            // Copy last evaluated value into inner so property retains it.
            auto& inner = targets_[i].inner;
            if (inner) {
                auto source = evaluate();
                if (source) {
                    inner->copy_from(*source);
                }
            }
            auto result = std::move(inner);
            targets_.erase(targets_.begin() + i);
            if (targets_.empty()) {
                unsubscribe();
            }
            return result;
        }
    }
    return nullptr;
}

// IAny overrides

array_view<Uid> BindingImpl::get_compatible_types() const
{
    auto source = evaluate();
    if (source) {
        return source->get_compatible_types();
    }
    auto inner = first_inner();
    return inner ? inner->get_compatible_types() : array_view<Uid>{};
}

size_t BindingImpl::get_data_size(Uid type) const
{
    auto source = evaluate();
    if (source) {
        return source->get_data_size(type);
    }
    auto inner = first_inner();
    return inner ? inner->get_data_size(type) : 0;
}

ReturnValue BindingImpl::get_data(void* to, size_t toSize, Uid type) const
{
    LoopGuard guard(this, tls_evaluating, tls_eval_depth);
    if (guard.looped) {
        auto inner = first_inner();
        return inner ? inner->get_data(to, toSize, type) : ReturnValue::Fail;
    }

    auto source = evaluate();
    if (source) {
        return source->get_data(to, toSize, type);
    }
    auto inner = first_inner();
    return inner ? inner->get_data(to, toSize, type) : ReturnValue::Fail;
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
    if (source) {
        return source->clone();
    }
    auto inner = first_inner();
    return inner ? inner->clone() : nullptr;
}

// Private

IAny::ConstPtr BindingImpl::evaluate() const
{
    return data_ ? data_->evaluate() : nullptr;
}

void BindingImpl::ensure_handler()
{
    if (handler_) {
        return;
    }
    auto fn = instance().create<IFunction>(ClassId::Function);
    if (auto* internal = interface_cast<IFunctionInternal>(fn)) {
        // Use a weak self-reference so the trampoline safely no-ops if the
        // binding is destroyed while the handler is still registered on a
        // source event (e.g. when dep weak_ptrs expired during unsubscribe).
        using Weak = IInterface::WeakPtr;
        auto* weak = new Weak(get_self<IInterface>());
        auto* trampoline = +[](void* ctx, FnArgs) -> IAny::Ptr {
            if (auto locked = static_cast<Weak*>(ctx)->lock()) {
                auto* binding = static_cast<BindingImpl*>(interface_cast<IBindingInternal>(locked));
                if (binding) {
                    binding->on_source_changed();
                }
            }
            return nullptr;
        };
        auto* deleter = +[](void* ctx) { delete static_cast<Weak*>(ctx); };
        internal->set_owned_callback(weak, trampoline, deleter);
    }
    handler_ = fn;
}

void BindingImpl::subscribe()
{
    if (subscribed_ || !data_) {
        return;
    }

    ensure_handler();
    data_->subscribe(handler_);
    subscribed_ = true;
}

void BindingImpl::unsubscribe()
{
    if (!subscribed_ || !handler_ || !data_) {
        return;
    }

    data_->unsubscribe(handler_);
    subscribed_ = false;
}

void BindingImpl::on_source_changed()
{
    LoopGuard guard(this, tls_notifying, tls_notify_depth);
    if (guard.looped) {
        return;
    }

    if (data_) {
        data_->invalidate();
    }

    auto self = get_self<IAnyExtension>();
    const IAny* arg = interface_cast<IAny>(self);

    for (auto& entry : targets_) {
        auto owner = entry.owner.lock();
        if (!owner) {
            continue;
        }

        if (invoke_type_ == Deferred) {
            auto propWeak = IPropertyInternal::WeakPtr(interface_pointer_cast<IPropertyInternal>(owner));
            instance().queue_deferred_property({std::move(propWeak), nullptr});
        } else {
            auto* prop = interface_cast<IProperty>(owner);
            if (prop) {
                invoke_event(prop->on_changed(), arg);
            }
        }
    }
}

IAny::Ptr BindingImpl::first_inner() const
{
    return !targets_.empty() ? targets_[0].inner : nullptr;
}

} // namespace velk
