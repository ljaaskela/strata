#include "js_any.h"

#include <velk/ext/core_object.h>
#include <velk/common.h>
#include <velk/string.h>

#include <cstring>

namespace velk {

namespace {

// All types this IAny can convert to.
constexpr Uid compatible_types[] = {
    type_uid<float>(),
    type_uid<double>(),
    type_uid<int32_t>(),
    type_uid<uint32_t>(),
    type_uid<int64_t>(),
    type_uid<uint64_t>(),
    type_uid<int8_t>(),
    type_uid<uint8_t>(),
    type_uid<int16_t>(),
    type_uid<uint16_t>(),
    type_uid<bool>(),
    type_uid<string>(),
};

constexpr size_t compatible_count = sizeof(compatible_types) / sizeof(compatible_types[0]);

} // namespace

JsAny::~JsAny()
{
    if (ctx_ && !JS_IsUndefined(val_)) {
        JS_FreeValue(ctx_, val_);
    }
}

void JsAny::set(JSContext* ctx, JSValue val)
{
    if (ctx_ && !JS_IsUndefined(val_)) {
        JS_FreeValue(ctx_, val_);
    }
    ctx_ = ctx;
    val_ = val;
}

void JsAny::invalidate()
{
    if (ctx_ && !JS_IsUndefined(val_)) {
        JS_FreeValue(ctx_, val_);
    }
    ctx_ = nullptr;
    val_ = JS_UNDEFINED;
}

array_view<Uid> JsAny::get_compatible_types() const
{
    return {compatible_types, compatible_count};
}

size_t JsAny::get_data_size(Uid type) const
{
    if (type == type_uid<float>()) return sizeof(float);
    if (type == type_uid<double>()) return sizeof(double);
    if (type == type_uid<int32_t>()) return sizeof(int32_t);
    if (type == type_uid<uint32_t>()) return sizeof(uint32_t);
    if (type == type_uid<int64_t>()) return sizeof(int64_t);
    if (type == type_uid<uint64_t>()) return sizeof(uint64_t);
    if (type == type_uid<int8_t>()) return sizeof(int8_t);
    if (type == type_uid<uint8_t>()) return sizeof(uint8_t);
    if (type == type_uid<int16_t>()) return sizeof(int16_t);
    if (type == type_uid<uint16_t>()) return sizeof(uint16_t);
    if (type == type_uid<bool>()) return sizeof(bool);
    if (type == type_uid<string>()) return sizeof(string);
    return 0;
}

ReturnValue JsAny::get_data(void* to, size_t toSize, Uid type) const
{
    if (!ctx_ || !to) {
        return ReturnValue::Fail;
    }

    if (type == type_uid<float>()) {
        if (toSize < sizeof(float)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        auto v = static_cast<float>(d);
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<double>()) {
        if (toSize < sizeof(double)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        std::memcpy(to, &d, sizeof(d));
        return ReturnValue::Success;
    }
    if (type == type_uid<int32_t>()) {
        if (toSize < sizeof(int32_t)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        auto v = static_cast<int32_t>(d);
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<uint32_t>()) {
        if (toSize < sizeof(uint32_t)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        auto v = static_cast<uint32_t>(d);
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<int64_t>()) {
        if (toSize < sizeof(int64_t)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        auto v = static_cast<int64_t>(d);
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<uint64_t>()) {
        if (toSize < sizeof(uint64_t)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        auto v = static_cast<uint64_t>(d);
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<int8_t>()) {
        if (toSize < sizeof(int8_t)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        auto v = static_cast<int8_t>(d);
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<uint8_t>()) {
        if (toSize < sizeof(uint8_t)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        auto v = static_cast<uint8_t>(d);
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<int16_t>()) {
        if (toSize < sizeof(int16_t)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        auto v = static_cast<int16_t>(d);
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<uint16_t>()) {
        if (toSize < sizeof(uint16_t)) return ReturnValue::Fail;
        double d = 0;
        JS_ToFloat64(ctx_, &d, val_);
        auto v = static_cast<uint16_t>(d);
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<bool>()) {
        if (toSize < sizeof(bool)) return ReturnValue::Fail;
        int b = JS_ToBool(ctx_, val_);
        bool v = b > 0;
        std::memcpy(to, &v, sizeof(v));
        return ReturnValue::Success;
    }
    if (type == type_uid<string>()) {
        if (toSize < sizeof(string)) return ReturnValue::Fail;
        const char* str = JS_ToCString(ctx_, val_);
        if (!str) return ReturnValue::Fail;
        auto* dest = static_cast<string*>(to);
        *dest = string(string_view(str, std::strlen(str)));
        JS_FreeCString(ctx_, str);
        return ReturnValue::Success;
    }

    return ReturnValue::Fail;
}

ReturnValue JsAny::set_data(void const*, size_t, Uid)
{
    // JsAny is read-only; the JS value is set via set().
    return ReturnValue::Fail;
}

ReturnValue JsAny::copy_from(const IAny&)
{
    return ReturnValue::Fail;
}

IAny::Ptr JsAny::clone() const
{
    if (!ctx_) return nullptr;
    auto ptr = ext::make_object<JsAny, IAny>();
    if (auto* c = static_cast<JsAny*>(ptr.get())) {
        c->set(ctx_, JS_DupValue(ctx_, val_));
    }
    return ptr;
}

} // namespace velk
