#ifndef VELK_VARIANT_H
#define VELK_VARIANT_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_variant.h>
#include <velk/interface/types.h>
#include <velk/vector.h>

namespace velk {

/**
 * @brief IAny implementation that can store any type and convert between compatible types on read.
 *
 * Wraps an inner typed IAny (e.g. AnyValue<float>) and replaces it when a different type is set.
 * Provides numeric conversions between bool, int32_t, int64_t, uint32_t, uint64_t, float, double.
 */
class VariantImpl final : public ext::ObjectCore<VariantImpl, IVariant>
{
public:
    VELK_CLASS_UID(ClassId::Variant);

    // IAny
    array_view<Uid> get_compatible_types() const override;
    size_t get_data_size(Uid type) const override;
    ReturnValue get_data(void* to, size_t toSize, Uid type) const override;
    ReturnValue set_data(const void* from, size_t fromSize, Uid type) override;
    ReturnValue copy_from(const IAny& other) override;
    IAny::Ptr clone() const override;

    // IVariant
    Uid stored_type() const override;
    bool can_convert_to(Uid type) const override;

private:
    void rebuild_compatible_cache(Uid current) const;

    IAny::Ptr stored_;
    mutable vector<Uid> compatible_cache_;
    mutable Uid cached_type_{};
};

} // namespace velk

#endif // VELK_VARIANT_H
