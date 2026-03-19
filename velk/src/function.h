#ifndef FUNCTION_H
#define FUNCTION_H

#include <velk/common.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_event.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/intf_object_storage.h>
#include <velk/interface/types.h>

namespace velk {

/**
 * @brief Internal interface for configuring an IFunction's invoke callback.
 *
 * Supports two dispatch mechanisms:
 * - @c set_invoke_callback() for explicit function pointer callbacks (highest priority).
 * - @c bind() for trampoline-based virtual dispatch, used by VELK_INTERFACE
 *   to route @c invoke() calls to @c fn_Name() virtual methods on the interface.
 */
class IFunctionInternal : public Interface<IFunctionInternal, IEvent>
{
public:
    /** @brief Sets the callback that will be called when IFunction::invoke is called. */
    virtual void set_invoke_callback(IFunction::CallableFn* fn) = 0;

    /**
     * @brief Binds a context pointer and trampoline function for virtual dispatch.
     * @param context Pointer to the interface subobject (passed as first arg to fn).
     * @param fn Static trampoline that casts context and calls the virtual method.
     */
    virtual void bind(void* context, IFunction::BoundFn* fn) = 0;

    /**
     * @brief Sets an owned callback with a heap-allocated context.
     *
     * Takes ownership of @p context. The @p deleter is called on @p context
     * when the function is destroyed or a new callback is set.
     *
     * @param context Heap-allocated callable (ownership transferred).
     * @param fn Static trampoline that casts context and invokes it.
     * @param deleter Static function that deletes context.
     */
    virtual void set_owned_callback(void* context, IFunction::BoundFn* fn,
                                    IFunction::ContextDeleter* deleter) = 0;
};

} // namespace velk

namespace velk::impl {

/**
 * @brief Lightweight IFunction/IFunctionInternal implementation.
 *
 * Supports three dispatch mechanisms for invoke():
 * 1. **Bound trampoline** (bind()): routes through a static trampoline to a
 *    virtual fn_Name() method on the owning interface. Used by VELK_INTERFACE.
 * 2. **Raw callback** (set_invoke_callback()): direct function pointer dispatch.
 * 3. **Owned callback** (set_owned_callback()): heap-allocated callable with
 *    type-erased context and deleter. Used by Callback for capturing lambdas.
 *
 * Does not support event handlers. Use EventImpl (ClassId::Event) for event
 * functionality with add_handler/remove_handler support.
 */
class Function final : public ext::ObjectCore<Function, IFunctionInternal>
{
public:
    VELK_CLASS_UID(ClassId::Function, "Function");

    Function() = default;
    ~Function();

public: // IStorageOwned
    IObjectStorage* get_owner() const override { return owner_; }
    size_t get_storage_id() const override { return storage_id_; }
    void set_owner(IObjectStorage* storage, size_t id) override
    {
        if (!owner_) {
            owner_ = storage;
            storage_id_ = id;
        }
    }

public: // IFunction
    IAny::Ptr invoke(FnArgs args, InvokeType type = Auto) const override;

public: // IFunctionInternal
    void set_invoke_callback(IFunction::CallableFn* fn) override;
    void bind(void* context, IFunction::BoundFn* fn) override;
    void set_owned_callback(void* context, IFunction::BoundFn* fn,
                            IFunction::ContextDeleter* deleter) override;

public: // IEvent (stubs; use EventImpl for handler support)
    ReturnValue add_handler(const IFunction::ConstPtr& fn, InvokeType type = Auto) const override;
    ReturnValue remove_handler(const IFunction::ConstPtr& fn) const override;
    bool has_handlers() const override;

private:
    static IAny::Ptr callback_trampoline(void* ctx, FnArgs args);

    void release_owned_context();

    IObjectStorage* owner_{};
    size_t storage_id_{};
    void* target_context_{}; ///< Context for target_fn_: interface ptr (bind) or CallableFn*
                             ///< (set_invoke_callback).
    IFunction::BoundFn*
        target_fn_{}; ///< Primary invoke target. Uses callback_trampoline when set via set_invoke_callback.
    void* owned_context_{};                        ///< Heap-allocated callable context (owned).
    IFunction::ContextDeleter* context_deleter_{}; ///< Deleter for owned_context_.
};

} // namespace velk::impl

#endif // FUNCTION_H
