#ifndef OBJECT_STORAGE_H
#define OBJECT_STORAGE_H

#include <velk/ext/refcounted_dispatch.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/intf_metadata_observer.h>
#include <velk/interface/intf_object_storage.h>
#include <velk/string.h>
#include <velk/vector.h>

#include <cassert>
#include <cstring>
#include <limits>

namespace velk {

/** @brief Per-member data, discriminated by MemberKind via members_[member_index].kind. */
union MemberData
{
    MemberData() { std::memset((void*)this, 0, sizeof(*this)); }
    ~MemberData() {} // Destruction managed by ObjectStorage::destroy_slot().

    struct property_t
    {
        IAny::Ptr data;    ///< The property's type-erased value.
        IEvent::Ptr event; ///< Lazily created on_changed event.
    } property;
    struct callback_t
    {
        void* target_context;                       ///< Dispatch target (interface ptr or CallableFn*).
        IFunction::BoundFn* target_fn;              ///< Primary invoke target.
        void* owned_context;                        ///< Heap-allocated callable context (owned).
        IFunction::ContextDeleter* context_deleter; ///< Deleter for owned_context.
    } callback;
};

/** @brief Per-member slot holding a lazily created instance and its associated data. */
struct MemberSlot
{
    MemberSlot() = default;
    explicit MemberSlot(IInterface::Ptr inst) : instance(std::move(inst)) {}
    ~MemberSlot() {} // Destruction managed by ObjectStorage::destroy_slot().
    MemberSlot(MemberSlot&& o) noexcept : instance(std::move(o.instance))
    {
        std::memcpy((void*)&data, &o.data, sizeof(data));
        std::memset((void*)&o.data, 0, sizeof(o.data));
    }
    MemberSlot& operator=(MemberSlot&&) = delete;
    MemberSlot(const MemberSlot&) = delete;
    MemberSlot& operator=(const MemberSlot&) = delete;

    IInterface::Ptr instance; ///< The created Property/Event/Function (null until first access).
    MemberData data{}; ///< Per-kind data: property value+event or function callback.
};

/** @brief Owned storage for a dynamically added property's metadata. */
struct DynamicMemberDesc
{
    string name_storage;       ///< Owned copy of the property name.
    MemberDesc desc{};         ///< desc.name points into name_storage.
    PropertyKind property_kind{}; ///< Owned PropertyKind (typeUid, no createRef).
};

/**
 * @brief Runtime object storage that lazily creates property/event/function instances
 *        and supports arbitrary attachments.
 *
 * Holds a static array_view<MemberDesc> (from VELK_INTERFACE) and creates runtime
 * impl::Property/impl::Function instances on first access via get_property()/get_event()/
 * get_function(). Created instances are cached in metadata_ slots.
 *
 * Does not inherit RefCountedDispatch; lifetime is managed by the owning Object
 * (allocated by VelkInstance at construction, deleted in Object's destructor).
 */
class ObjectStorage final : public ext::InterfaceDispatch<IObjectStorage>
{
public:
    /**
     * @brief Constructs an object storage from static member descriptors.
     * @param members The static metadata array (from VELK_INTERFACE).
     * @param owner The owning object, used to bind function trampolines and resolve property state.
     */
    explicit ObjectStorage(array_view<MemberDesc> members, IInterface* owner = nullptr);
    ~ObjectStorage() override;

public: // IObject (inherited via IObjectStorage; not used as an IObject)
    Uid get_class_uid() const override { return {}; }
    string_view get_class_name() const override { return {}; }
    IObject::Ptr get_self() const override { return {}; }
    uint32_t get_object_flags() const override { return 0; }

public: // IPropertyState (inherited via IMetadata; state lives in the Object, not here)
    void* get_property_state(Uid) override { return nullptr; }

public: // IMetadata
    array_view<MemberDesc> get_static_metadata() const override;
    IProperty::Ptr get_property(string_view name, Uid interfaceUid,
                                Resolve mode) const override;
    IEvent::Ptr get_event(string_view name, Resolve mode = Resolve::Create) const override;
    IFunction::Ptr get_function(string_view name, Resolve mode = Resolve::Create) const override;
    void notify(MemberKind kind, Uid interfaceUid, Notification notification) const override;
    IProperty::Ptr add_property(string_view name, Uid typeUid,
                                const IAny* defaultValue) const override;
    ReturnValue remove_property(string_view name) const override;
    vector<PropertyInfo> get_properties() const override;

public: // IObjectStorage (attachment operations)
    ReturnValue add_attachment(const IInterface::Ptr& attachment) override;
    ReturnValue remove_attachment(const IInterface::Ptr& attachment) override;
    size_t attachment_count() const override;
    IInterface::Ptr get_attachment(size_t index) const override;
    IInterface::Ptr find_attachment(const AttachmentQuery& query, Resolve mode) override;
    vector<IInterface::Ptr> find_attachments(const AttachmentQuery& query) const override;

public: // IObjectStorage (property events + observers)
    IEvent::Ptr get_property_event(size_t storage_id, Resolve mode) const override;
    void invoke_property_changed(size_t storage_id, IProperty* property) const override;
    void add_observer(IMetadataObserver* observer) override;
    void remove_observer(IMetadataObserver* observer) override;

public:
    static constexpr uint16_t DynamicBit = 0x8000;

    /** @brief Returns the metadata slot at the given index. High bit set = dynamic slot. */
    MemberSlot& slot(size_t id) const
    {
        if (id & DynamicBit) {
            return dynamic_metadata_[id & ~DynamicBit];
        }
        return metadata_[id];
    }
    /** @brief Returns the MemberData at the given index. */
    MemberData& data(size_t id) const
    {
        return slot(id).data;
    }

private:
    array_view<MemberDesc> members_; ///< Static metadata descriptors from VELK_INTERFACE.
    IInterface* owner_{};            ///< Owning object for trampoline binding and state access.

    mutable vector<IInterface::Ptr> attachments_;      ///< Object attachments
    mutable vector<MemberSlot> metadata_;              ///< Sparse: one slot per lazily created member.
    mutable vector<DynamicMemberDesc> dynamic_members_; ///< Dynamically added property descriptors.
    mutable vector<MemberSlot> dynamic_metadata_;       ///< Slots for dynamic properties.

    /// Object-level observers for property change notifications.
    vector<IMetadataObserver*> observers_;

    /** @brief Finds a static member by name and kind, creating its runtime instance if needed. */
    IInterface::Ptr find_or_create(string_view name, MemberKind kind, Resolve mode,
                                   Uid interfaceUid = {}) const;
    /** @brief Creates a runtime instance (impl::Property or impl::Function) from a member descriptor. */
    IInterface::Ptr create(MemberDesc desc) const;
    /** @brief Binds a function trampoline directly into the slot's callback data. */
    void bind(const MemberDesc& m, uint16_t slot_index) const;
    /** @brief Releases owned callback in a slot. */
    void release_callback(MemberSlot& slot) const;
    /** @brief Destroys the union arm of a slot based on its member kind. */
    void destroy_slot(MemberSlot& slot) const;
    /** @brief Destroys a dynamic property slot (always a Property with owned data). */
    void destroy_dynamic_slot(MemberSlot& slot) const;
    /** @brief Finds an existing dynamic property by name, or creates its wrapper. */
    IProperty::Ptr find_dynamic_property(string_view name, Resolve mode) const;
    /** @brief Notifies all observers_ */
    void notify_observers(string_view name, Uid interfaceUid) const;
};

} // namespace velk

#endif // OBJECT_STORAGE_H
