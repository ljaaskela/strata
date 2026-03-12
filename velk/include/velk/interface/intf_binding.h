#ifndef VELK_INTF_BINDING_H
#define VELK_INTF_BINDING_H

#include <velk/interface/intf_any_extension.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/intf_property.h>
#include <velk/vector.h>

namespace velk {

/**
 * @brief Interface for property bindings.
 *
 * A binding is an IAnyExtension that intercepts property reads to return values
 * from a source property or computed function. It can be installed on multiple
 * target properties; all targets read the same evaluated value.
 */
class IBinding : public Interface<IBinding, IAnyExtension>
{
public:
    /** @brief Returns the source property, or null if this is a function binding. */
    virtual IProperty::ConstPtr get_source_property() const = 0;
    /** @brief Returns the source function, or null if this is a property binding. */
    virtual IFunction::ConstPtr get_source_function() const = 0;

    /** @brief Installs this binding on a target property. Returns false on type mismatch. */
    virtual bool add_target(const IProperty::Ptr& target) = 0;
    /** @brief Removes this binding from a target property. Returns false if not installed. */
    virtual bool remove_target(const IProperty::Ptr& target) = 0;
    /** @brief Removes this binding from all target properties. */
    virtual void uninstall() = 0;
};

/**
 * @brief Internal interface for configuring a binding's source.
 *
 * Used by the API helpers to set up the binding after creation.
 */
class IBindingInternal : public Interface<IBindingInternal, IBinding>
{
public:
    /** @brief Configures this binding to read from a source property. */
    virtual void set_source_property(const IProperty::ConstPtr& source) = 0;
    /** @brief Configures this binding to evaluate a function with explicit deps. */
    virtual void set_source_function(const IFunction::ConstPtr& fn, vector<IProperty::ConstPtr> deps) = 0;
    /** @brief Configures this binding to evaluate a function with auto-detected deps. */
    virtual void set_source_function(const IFunction::ConstPtr& fn) = 0;
    /**
     * @brief Sets how the binding propagates source changes to the target.
     *
     * Immediate (default): fires target on_changed synchronously when source changes.
     * Deferred: queues a notification for the next update() call, coalescing
     * intermediate changes into a single evaluation per frame.
     */
    virtual void set_invoke_type(InvokeType type) = 0;
};

} // namespace velk

#endif // VELK_INTF_BINDING_H
