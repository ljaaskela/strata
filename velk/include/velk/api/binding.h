#ifndef VELK_API_BINDING_H
#define VELK_API_BINDING_H

#include <velk/api/velk.h>
#include <velk/interface/intf_any_extension.h>
#include <velk/interface/intf_binding.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/types.h>
#include <velk/vector.h>

#include <initializer_list>

namespace velk {

/**
 * @brief Lightweight wrapper around an installed binding and its target property.
 *
 * A Binding has a 1:1 relationship with its target. Provides null-safe access
 * to binding introspection (source property/function) and an unbind() method
 * to remove the binding from the target.
 */
class Binding
{
public:
    /** @brief Wraps an existing binding and its target. */
    explicit Binding(IBinding::Ptr binding, IProperty::Ptr target)
        : binding_(std::move(binding)),
          target_(target)
    {
    }

    /** @brief Constructs a null binding. */
    Binding() = default;

    /** @brief Returns true if the underlying binding is valid. */
    operator bool() const { return binding_.operator bool(); }

    /** @brief Implicit conversion to IBinding::Ptr. */
    operator IBinding::Ptr() { return binding_; }
    operator const IBinding::ConstPtr() const { return binding_; }

    /** @brief Returns the target property this binding is installed on. */
    IProperty::Ptr get_target_property() const { return target_.lock(); }

    /** @brief Returns the source property, or null if this is a function binding. */
    IProperty::ConstPtr get_source_property() const
    {
        return binding_ ? binding_->get_source_property() : nullptr;
    }

    /** @brief Returns the source function, or null if this is a property binding. */
    IFunction::ConstPtr get_source_function() const
    {
        return binding_ ? binding_->get_source_function() : nullptr;
    }

    /**
     * @brief Removes this binding from the target property.
     *
     * On removal, the property retains the last bound value and writes become
     * possible again.
     *
     * @return true if the binding was found and removed.
     */
    bool unbind()
    {
        if (!installed_) {
            return false;
        }
        installed_ = false;
        auto ext = interface_pointer_cast<IAnyExtension>(binding_);
        if (!ext) {
            return false;
        }
        auto target = target_.lock();
        auto* internal = interface_cast<IPropertyInternal>(target);
        return internal && internal->remove_extension(ext);
    }

    /**
     * @brief Bind the value of the target property to the value of source.
     *
     * If already installed, unbinds first before rebinding.
     *
     * @param source The property to bind from.
     * @param type Type of the binding.
     * @return true if binding was successfully installed.
     */
    bool bind(const IProperty::ConstPtr& source, InvokeType type)
    {
        unbind();
        auto target = target_.lock();
        if (auto* internal = get_internal(); internal && target && source) {
            internal->set_source_property(source);
            internal->set_invoke_type(type);
            auto ext = interface_pointer_cast<IAnyExtension>(binding_);
            if (auto pi = interface_cast<IPropertyInternal>(target); pi && ext) {
                installed_ = pi->install_extension(ext);
                return installed_;
            }
        }
        return false;
    }

    /**
     * @brief Bind the value of the target property to a computed function result.
     *
     * If already installed, unbinds first before rebinding.
     *
     * @param fn The function that computes the value.
     * @param deps Properties passed as arguments to the function.
     * @param type Type of the binding.
     * @return true if binding was successfully installed.
     */
    bool bind(const IFunction::ConstPtr& fn, vector<IProperty::ConstPtr> deps, InvokeType type)
    {
        unbind();
        auto target = target_.lock();
        if (auto* internal = get_internal(); internal && target && fn) {
            internal->set_source_function(fn, std::move(deps));
            internal->set_invoke_type(type);
            auto ext = interface_pointer_cast<IAnyExtension>(binding_);
            if (auto pi = interface_cast<IPropertyInternal>(target); pi && ext) {
                installed_ = pi->install_extension(ext);
                return installed_;
            }
        }
        return false;
    }

private:
    IBindingInternal* get_internal() { return interface_cast<IBindingInternal>(binding_); }
    const IBindingInternal* get_internal() const { return interface_cast<IBindingInternal>(binding_); }

    IBinding::Ptr binding_;
    IProperty::WeakPtr target_;
    bool installed_ = false;
};

/**
 * @brief Creates a binding (not yet installed) associated with a target property.
 * @param target The target property to bind to.
 * @return The binding, or null if target is null.
 */
inline Binding create_binding(const IProperty::Ptr& target)
{
    return target ? Binding(instance().create<IBinding>(ClassId::Binding), target) : Binding{};
}

/**
 * @brief Creates a property-to-property binding and installs it on the target.
 *
 * While bound, reads from @p target return the source property's current value,
 * and writes to @p target are rejected. Source on_changed events propagate to
 * the target.
 *
 * @param target The property to bind (must support IPropertyInternal).
 * @param source The source property whose value will be read.
 * @param type Immediate (default) fires on_changed synchronously; Deferred batches to update().
 * @return The installed Binding, or a null Binding on failure.
 */
inline Binding bind(const IProperty::Ptr& target, const IProperty::ConstPtr& source,
                    InvokeType type = Immediate)
{
    auto b = create_binding(target);
    return b.bind(source, type) ? b : Binding{};
}

/**
 * @brief Creates a function binding with explicit deps and installs it on the target.
 *
 * The function is invoked with dependency values as FnArgs. The result is cached
 * and returned on reads. When any dependency changes, the cache is invalidated
 * and the target's on_changed fires.
 *
 * @param target The property to bind.
 * @param fn The function that computes the value.
 * @param deps Properties passed as arguments to the function.
 * @param type Immediate (default) fires on_changed synchronously; Deferred batches to update().
 * @return The installed Binding, or a null Binding on failure.
 */
inline Binding bind(const IProperty::Ptr& target, const IFunction::ConstPtr& fn,
                    std::initializer_list<IProperty::ConstPtr> deps, InvokeType type = Immediate)
{
    auto b = create_binding(target);
    return b.bind(fn, vector<IProperty::ConstPtr>(deps.begin(), deps.end()), type) ? b : Binding{};
}

} // namespace velk

#endif // VELK_API_BINDING_H
