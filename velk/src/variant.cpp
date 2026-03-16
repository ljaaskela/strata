#include "variant.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_variant.h>

#include <cstdint>
#include <cstring>

namespace velk::impl {

namespace {

struct ConversionEntry
{
    Uid from;
    Uid to;
    size_t fromSize;
    size_t toSize;
    void (*convert)(const void* from, void* to);
};

template <typename From, typename To>
void numeric_convert(const void* from, void* to)
{
    *static_cast<To*>(to) = static_cast<To>(*static_cast<const From*>(from));
}

#define CONV(From, To) \
    ConversionEntry { type_uid<From>(), type_uid<To>(), sizeof(From), sizeof(To), &numeric_convert<From, To> }

// All pairwise conversions between the 7 numeric types (42 entries)
static const ConversionEntry g_conversions[] = {
    CONV(bool, int32_t),
    CONV(bool, int64_t),
    CONV(bool, uint32_t),
    CONV(bool, uint64_t),
    CONV(bool, float),
    CONV(bool, double),

    CONV(int32_t, bool),
    CONV(int32_t, int64_t),
    CONV(int32_t, uint32_t),
    CONV(int32_t, uint64_t),
    CONV(int32_t, float),
    CONV(int32_t, double),

    CONV(int64_t, bool),
    CONV(int64_t, int32_t),
    CONV(int64_t, uint32_t),
    CONV(int64_t, uint64_t),
    CONV(int64_t, float),
    CONV(int64_t, double),

    CONV(uint32_t, bool),
    CONV(uint32_t, int32_t),
    CONV(uint32_t, int64_t),
    CONV(uint32_t, uint64_t),
    CONV(uint32_t, float),
    CONV(uint32_t, double),

    CONV(uint64_t, bool),
    CONV(uint64_t, int32_t),
    CONV(uint64_t, int64_t),
    CONV(uint64_t, uint32_t),
    CONV(uint64_t, float),
    CONV(uint64_t, double),

    CONV(float, bool),
    CONV(float, int32_t),
    CONV(float, int64_t),
    CONV(float, uint32_t),
    CONV(float, uint64_t),
    CONV(float, double),

    CONV(double, bool),
    CONV(double, int32_t),
    CONV(double, int64_t),
    CONV(double, uint32_t),
    CONV(double, uint64_t),
    CONV(double, float),
};

#undef CONV

static constexpr size_t g_num_conversions = sizeof(g_conversions) / sizeof(g_conversions[0]);

const ConversionEntry* find_conversion(Uid from, Uid to)
{
    for (size_t i = 0; i < g_num_conversions; ++i) {
        if (g_conversions[i].from == from && g_conversions[i].to == to) {
            return &g_conversions[i];
        }
    }
    return nullptr;
}

// All 7 numeric types. Any numeric stored type is compatible with all of them.
static const Uid g_all_numeric[] = {
    type_uid<bool>(),     type_uid<int32_t>(),  type_uid<int64_t>(), type_uid<uint32_t>(),
    type_uid<uint64_t>(), type_uid<float>(),    type_uid<double>()
};
static constexpr size_t g_num_numeric = sizeof(g_all_numeric) / sizeof(g_all_numeric[0]);

bool is_numeric(Uid type)
{
    for (size_t i = 0; i < g_num_numeric; ++i) {
        if (g_all_numeric[i] == type) {
            return true;
        }
    }
    return false;
}

} // namespace

Uid Variant::stored_type() const
{
    if (!stored_) {
        return {};
    }
    auto types = stored_->get_compatible_types();
    return types.empty() ? Uid{} : types[0];
}

bool Variant::can_convert_to(Uid type) const
{
    if (!stored_) {
        return false;
    }
    if (is_compatible(*stored_, type)) {
        return true;
    }
    Uid from = stored_type();
    return is_numeric(from) && is_numeric(type);
}

array_view<Uid> Variant::get_compatible_types() const
{
    if (!stored_) {
        return {};
    }
    Uid current = stored_type();
    if (is_numeric(current)) {
        return {g_all_numeric, g_num_numeric};
    }
    return stored_->get_compatible_types();
}

size_t Variant::get_data_size(Uid type) const
{
    if (!stored_) {
        return 0;
    }
    if (is_compatible(*stored_, type)) {
        return stored_->get_data_size(type);
    }
    auto* entry = find_conversion(stored_type(), type);
    return entry ? entry->toSize : 0;
}

ReturnValue Variant::get_data(void* to, size_t toSize, Uid type) const
{
    if (!stored_ || !to) {
        return ReturnValue::Fail;
    }
    // Direct type match
    if (is_compatible(*stored_, type)) {
        return stored_->get_data(to, toSize, type);
    }
    // Try conversion
    Uid from = stored_type();
    auto* entry = find_conversion(from, type);
    if (!entry || toSize < entry->toSize) {
        return ReturnValue::Fail;
    }
    // Read stored value into temp buffer, convert, write to output
    alignas(8) char buf[8];
    if (entry->fromSize > sizeof(buf)) {
        return ReturnValue::Fail;
    }
    if (failed(stored_->get_data(buf, entry->fromSize, from))) {
        return ReturnValue::Fail;
    }
    entry->convert(buf, to);
    return ReturnValue::Success;
}

ReturnValue Variant::set_data(const void* from, size_t fromSize, Uid type)
{
    if (!from) {
        return ReturnValue::Fail;
    }
    // If stored type matches, delegate
    if (stored_ && is_compatible(*stored_, type)) {
        return stored_->set_data(from, fromSize, type);
    }
    // Create a new typed any for the incoming type
    auto any = instance().create_any(type);
    if (!any) {
        return ReturnValue::Fail;
    }
    if (failed(any->set_data(from, fromSize, type))) {
        return ReturnValue::Fail;
    }
    stored_ = std::move(any);
    return ReturnValue::Success;
}

// Returns the primary stored type UID for any IAny (uses IVariant::stored_type if available,
// otherwise falls back to get_compatible_types()[0]).
static Uid primary_type(const IAny& any)
{
    if (auto* v = interface_cast<const IVariant>(&any)) {
        return v->stored_type();
    }
    auto types = any.get_compatible_types();
    return types.empty() ? Uid{} : types[0];
}

ReturnValue Variant::copy_from(const IAny& other)
{
    Uid src_type = primary_type(other);

    // If stored_ exists and matches the source type, delegate for value comparison
    if (stored_ && src_type != Uid{} && is_compatible(*stored_, src_type)) {
        return stored_->copy_from(other);
    }

    // Empty source
    if (src_type == Uid{}) {
        bool had = (stored_ != nullptr);
        stored_ = nullptr;
        return had ? ReturnValue::Success : ReturnValue::NothingToDo;
    }

    // If other is a Variant, create a typed any and copy through to avoid nesting
    if (auto* ov = interface_cast<const IVariant>(&other)) {
        Uid type = ov->stored_type();
        if (type == Uid{}) {
            bool had = (stored_ != nullptr);
            stored_ = nullptr;
            return had ? ReturnValue::Success : ReturnValue::NothingToDo;
        }
        auto any = instance().create_any(type);
        if (any && succeeded(any->copy_from(other))) {
            stored_ = std::move(any);
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }

    // Regular IAny: clone it
    stored_ = other.clone();
    return stored_ ? ReturnValue::Success : ReturnValue::Fail;
}

IAny::Ptr Variant::clone() const
{
    auto obj = ext::make_object<Variant>();
    if (stored_) {
        auto* impl = static_cast<Variant*>(obj.get());
        impl->stored_ = stored_->clone();
    }
    return interface_pointer_cast<IAny>(obj);
}

} // namespace velk::impl
