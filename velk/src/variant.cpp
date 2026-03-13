#include "variant.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_variant.h>

#include <cstdint>
#include <cstring>

namespace velk {

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

} // namespace

Uid VariantImpl::stored_type() const
{
    if (!stored_) {
        return {};
    }
    auto types = stored_->get_compatible_types();
    return types.empty() ? Uid{} : types[0];
}

bool VariantImpl::can_convert_to(Uid type) const
{
    if (!stored_) {
        return false;
    }
    if (is_compatible(*stored_, type)) {
        return true;
    }
    return find_conversion(stored_type(), type) != nullptr;
}

void VariantImpl::rebuild_compatible_cache(Uid current) const
{
    compatible_cache_.clear();
    if (stored_) {
        for (auto uid : stored_->get_compatible_types()) {
            compatible_cache_.push_back(uid);
        }
    }
    for (size_t i = 0; i < g_num_conversions; ++i) {
        if (g_conversions[i].from == current) {
            bool found = false;
            for (auto uid : compatible_cache_) {
                if (uid == g_conversions[i].to) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                compatible_cache_.push_back(g_conversions[i].to);
            }
        }
    }
    cached_type_ = current;
}

array_view<Uid> VariantImpl::get_compatible_types() const
{
    if (!stored_) {
        return {};
    }
    Uid current = stored_type();
    if (current != cached_type_) {
        rebuild_compatible_cache(current);
    }
    return {compatible_cache_.data(), compatible_cache_.size()};
}

size_t VariantImpl::get_data_size(Uid type) const
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

ReturnValue VariantImpl::get_data(void* to, size_t toSize, Uid type) const
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

ReturnValue VariantImpl::set_data(const void* from, size_t fromSize, Uid type)
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
    cached_type_ = {}; // Invalidate cache
    return ReturnValue::Success;
}

ReturnValue VariantImpl::copy_from(const IAny& other)
{
    // If stored_ exists and is compatible with the source's primary type, delegate for value comparison
    if (stored_) {
        auto other_types = other.get_compatible_types();
        if (!other_types.empty()) {
            Uid src_type = other_types[0];
            if (is_compatible(*stored_, src_type)) {
                return stored_->copy_from(other);
            }
        }
    }

    // Different type or no stored_
    auto types = other.get_compatible_types();
    if (types.empty()) {
        bool had = (stored_ != nullptr);
        stored_ = nullptr;
        cached_type_ = {};
        return had ? ReturnValue::Success : ReturnValue::NothingToDo;
    }

    // If other is a Variant, create a typed any and copy through to avoid nesting
    if (interface_cast<const IVariant>(&other)) {
        Uid type = types[0];
        auto any = instance().create_any(type);
        if (any && succeeded(any->copy_from(other))) {
            stored_ = std::move(any);
            cached_type_ = {};
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }

    // Regular IAny: clone it
    stored_ = other.clone();
    cached_type_ = {};
    return stored_ ? ReturnValue::Success : ReturnValue::Fail;
}

IAny::Ptr VariantImpl::clone() const
{
    auto obj = ext::make_object<VariantImpl>();
    if (stored_) {
        auto* impl = static_cast<VariantImpl*>(obj.get());
        impl->stored_ = stored_->clone();
    }
    return interface_pointer_cast<IAny>(obj);
}

} // namespace velk
