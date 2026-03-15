#ifndef VELK_OBJECT_REF_IMPL_H
#define VELK_OBJECT_REF_IMPL_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_object_ref.h>
#include <velk/interface/types.h>

namespace velk::impl {

/**
 * @brief IObjectRef implementation that stores a reference to another IObject.
 *
 * Maintains both a strong and weak pointer. The owning flag controls which is
 * active: when owning (default), the strong ref keeps the target alive; when
 * non-owning, only the weak ref is held and get_object() locks it on access.
 *
 * get_data/set_data operate on IObject::Ptr values.
 */
class ObjectRef final : public ext::ObjectCore<ObjectRef, IObjectRef>
{
public:
    VELK_CLASS_UID(ClassId::ObjectRef, "ObjectRef");

    // IAny
    array_view<Uid> get_compatible_types() const override;
    size_t get_data_size(Uid type) const override;
    ReturnValue get_data(void* to, size_t toSize, Uid type) const override;
    ReturnValue set_data(const void* from, size_t fromSize, Uid type) override;
    ReturnValue copy_from(const IAny& other) override;
    IAny::Ptr clone() const override;

    // IObjectRef
    IObject::Ptr get_object() const override;
    ReturnValue set_object(const IObject::Ptr& obj) override;
    bool is_owning() const override;
    ReturnValue set_owning(bool owning) override;
    Uid constraint_uid() const override;
    ReturnValue set_constraint(Uid uid) override;

private:
    IObject::Ptr strong_;
    IObject::WeakPtr weak_;
    Uid constraint_{};
    bool owning_{true};
};

} // namespace velk::impl

#endif // VELK_OBJECT_REF_IMPL_H
