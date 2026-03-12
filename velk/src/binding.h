#ifndef VELK_BINDING_IMPL_H
#define VELK_BINDING_IMPL_H

#include <velk/ext/any_extension.h>
#include <velk/ext/event.h>
#include <velk/interface/intf_binding.h>
#include <velk/interface/types.h>
#include <velk/vector.h>

#include <memory>

namespace velk {

/**
 * @brief Mode-specific binding data. Subclassed for property vs function binding.
 *
 * BindingImpl delegates evaluate/subscribe/unsubscribe to the active data object.
 * The data object is replaced when the binding mode changes.
 */
struct BindingData
{
    virtual ~BindingData() = default;

    /** @brief Evaluates the source value. */
    virtual IAny::ConstPtr evaluate() const = 0;

    /** @brief Subscribes handler to source on_changed events. */
    virtual void subscribe(const IFunction::Ptr& handler) = 0;

    /** @brief Unsubscribes handler from source on_changed events. */
    virtual void unsubscribe(const IFunction::Ptr& handler) = 0;

    /** @brief Returns the source property, or null. */
    virtual IProperty::ConstPtr get_source_property() const { return nullptr; }

    /** @brief Returns the source function, or null. */
    virtual IFunction::ConstPtr get_source_function() const { return nullptr; }

    /** @brief Invalidates any cached result. */
    virtual void invalidate() {}
};

/**
 * @brief IAnyExtension that intercepts property reads to return values from a
 *        source property or computed function result.
 *
 * Mode-specific state (source property, function, deps, cache) is held in a
 * BindingData subclass, keeping BindingImpl itself lightweight and allowing
 * runtime mode switching.
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
    IAny::ConstPtr evaluate() const;
    void ensure_handler();
    void subscribe();
    void unsubscribe();
    void on_source_changed();

    std::unique_ptr<BindingData> data_;
    IInterface::WeakPtr owner_;
    IFunction::Ptr handler_;
    InvokeType invoke_type_ = Immediate;
    bool subscribed_ = false;
};

} // namespace velk

#endif // VELK_BINDING_IMPL_H
