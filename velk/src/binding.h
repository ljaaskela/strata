#ifndef VELK_BINDING_IMPL_H
#define VELK_BINDING_IMPL_H

#include <velk/ext/any_extension.h>
#include <velk/ext/event.h>
#include <velk/interface/intf_binding.h>
#include <velk/interface/types.h>
#include <velk/vector.h>

namespace velk {

/**
 * @brief IAnyExtension that intercepts property reads to return values from a
 *        source property or computed function result.
 *
 * Two modes:
 *   Property-to-property: get_data delegates to the source property's value.
 *   Function with deps:   get_data invokes the function with dep values, caches result.
 *
 * Writes (set_data/copy_from) return Fail while the binding is active.
 * Subscribes to source on_changed events and propagates them to the target property.
 */
class BindingImpl final : public ext::AnyExtension<BindingImpl, IBindingInternal>
{
public:
    VELK_CLASS_UID(ClassId::Binding);

    ~BindingImpl();

    // IBinding
    IProperty::ConstPtr get_source_property() const override;
    IFunction::ConstPtr get_source_function() const override;

    // IBindingInternal
    void set_source_property(const IProperty::ConstPtr& source) override;
    void set_source_function(const IFunction::ConstPtr& fn, vector<IProperty::ConstPtr> deps) override;
    void set_source_function(const IFunction::ConstPtr& fn) override;
    void set_invoke_type(InvokeType type) override;

    // IAnyExtension overrides
    bool set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner) override;
    IAny::Ptr take_inner(IInterface& owner) override;

    // IAny overrides (intercept reads, block writes)
    array_view<Uid> get_compatible_types() const override;
    size_t get_data_size(Uid type) const override;
    ReturnValue get_data(void* to, size_t toSize, Uid type) const override;
    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override;
    ReturnValue copy_from(const IAny& other) override;
    IAny::Ptr clone() const override;

private:
    /** @brief Evaluates the source value (property or function). Returns null on loop/error. */
    IAny::ConstPtr evaluate() const;
    /** @brief Evaluates with auto-dependency tracking. Resubscribes if deps change. */
    IAny::ConstPtr evaluate_auto_track() const;
    /** @brief Creates the handler function once (lazy). */
    void ensure_handler();
    /** @brief Subscribes to source/dep on_changed events. */
    void subscribe();
    /** @brief Unsubscribes from all source/dep on_changed events. */
    void unsubscribe();
    /** @brief Called when any source/dep changes. Fires target property's on_changed. */
    void on_source_changed();

    IProperty::ConstPtr source_property_;
    IFunction::ConstPtr source_function_;
    vector<IProperty::ConstPtr> deps_;
    mutable IAny::Ptr cached_result_;
    mutable bool cache_valid_ = false;
    bool auto_track_ = false; ///< True if deps should be auto-detected on evaluate.
    IInterface::WeakPtr owner_;
    IFunction::Ptr handler_;
    InvokeType invoke_type_ = Immediate;
    bool subscribed_ = false;
};

} // namespace velk

#endif // VELK_BINDING_IMPL_H
