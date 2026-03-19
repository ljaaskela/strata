#ifndef EVENT_H
#define EVENT_H

#include "function.h"

#include <velk/interface/intf_object_storage.h>
#include <velk/interface/types.h>
#include <velk/vector.h>

namespace velk::impl {

/**
 * @brief Default IEvent implementation with handler management.
 *
 * Extends the invoke machinery of FunctionImpl with a partitioned handler
 * list: [0, deferred_begin_) for immediate handlers, [deferred_begin_, size())
 * for deferred handlers.
 */
class Event final : public ext::ObjectCore<Event, IFunctionInternal>
{
public:
    VELK_CLASS_UID(ClassId::Event, "Event");

    Event() = default;
    ~Event();

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

public: // IEvent
    ReturnValue add_handler(const IFunction::ConstPtr& fn, InvokeType type = Auto) const override;
    ReturnValue remove_handler(const IFunction::ConstPtr& fn) const override;
    bool has_handlers() const override;

private:
    static IAny::Ptr callback_trampoline(void* ctx, FnArgs args);
    void invoke_handlers(FnArgs args) const;
    array_view<IFunction::ConstPtr> immediate_handlers() const;
    array_view<IFunction::ConstPtr> deferred_handlers() const;

    void release_owned_context();

    IObjectStorage* owner_{};
    size_t storage_id_{};
    void* target_context_{};
    IFunction::BoundFn* target_fn_{};
    void* owned_context_{};
    IFunction::ContextDeleter* context_deleter_{};
    /// Partitioned handler list: [0, deferred_begin_) = immediate, [deferred_begin_, size()) = deferred.
    mutable vector<IFunction::ConstPtr> handlers_;
    mutable uint32_t deferred_begin_{};
};

} // namespace velk::impl

#endif // EVENT_H
