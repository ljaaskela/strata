#include "js_marshal.h"

#include <velk/api/velk.h>
#include <velk/common.h>
#include <velk/string.h>

namespace velk {

JSValue any_to_jsvalue(JSContext* ctx, const IAny* any)
{
    if (!any) {
        return JS_UNDEFINED;
    }
    auto types = any->get_compatible_types();
    if (types.empty()) {
        return JS_UNDEFINED;
    }
    Uid uid = types[0];

    if (uid == type_uid<float>()) {
        float v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewFloat64(ctx, v);
        }
    } else if (uid == type_uid<double>()) {
        double v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewFloat64(ctx, v);
        }
    } else if (uid == type_uid<int32_t>()) {
        int32_t v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewInt32(ctx, v);
        }
    } else if (uid == type_uid<uint32_t>()) {
        uint32_t v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewUint32(ctx, v);
        }
    } else if (uid == type_uid<int64_t>()) {
        int64_t v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewInt64(ctx, v);
        }
    } else if (uid == type_uid<uint64_t>()) {
        uint64_t v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewFloat64(ctx, static_cast<double>(v));
        }
    } else if (uid == type_uid<int8_t>()) {
        int8_t v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewInt32(ctx, v);
        }
    } else if (uid == type_uid<uint8_t>()) {
        uint8_t v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewInt32(ctx, v);
        }
    } else if (uid == type_uid<int16_t>()) {
        int16_t v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewInt32(ctx, v);
        }
    } else if (uid == type_uid<uint16_t>()) {
        uint16_t v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewInt32(ctx, v);
        }
    } else if (uid == type_uid<bool>()) {
        bool v{};
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewBool(ctx, v);
        }
    } else if (uid == type_uid<string>()) {
        string v;
        if (any->get_data(&v, sizeof(v), uid) == ReturnValue::Success) {
            return JS_NewStringLen(ctx, v.c_str(), v.size());
        }
    }
    return JS_UNDEFINED;
}

namespace {

template <class T>
IAny::Ptr make_any(const T& value)
{
    auto any = ::velk::instance().create_any(type_uid<T>());
    if (any) {
        any->set_data(&value, sizeof(T), type_uid<T>());
    }
    return any;
}

IAny::Ptr make_string_any(const char* str, size_t len)
{
    auto any = ::velk::instance().create_any(type_uid<string>());
    if (any) {
        string s(str, len);
        any->set_data(&s, sizeof(s), type_uid<string>());
    }
    return any;
}

} // namespace

IAny::Ptr jsvalue_to_any(JSContext* ctx, JSValue val, Uid target_type)
{
    if (JS_IsUndefined(val) || JS_IsNull(val)) {
        return nullptr;
    }

    if (target_type == type_uid<float>()) {
        double d;
        if (JS_ToFloat64(ctx, &d, val) == 0) {
            return make_any(static_cast<float>(d));
        }
    } else if (target_type == type_uid<double>()) {
        double d;
        if (JS_ToFloat64(ctx, &d, val) == 0) {
            return make_any(d);
        }
    } else if (target_type == type_uid<int32_t>()) {
        int32_t v;
        if (JS_ToInt32(ctx, &v, val) == 0) {
            return make_any(v);
        }
    } else if (target_type == type_uid<uint32_t>()) {
        uint32_t v;
        if (JS_ToUint32(ctx, &v, val) == 0) {
            return make_any(v);
        }
    } else if (target_type == type_uid<int64_t>()) {
        int64_t v;
        if (JS_ToInt64(ctx, &v, val) == 0) {
            return make_any(v);
        }
    } else if (target_type == type_uid<uint64_t>()) {
        int64_t v;
        if (JS_ToInt64(ctx, &v, val) == 0) {
            return make_any(static_cast<uint64_t>(v));
        }
    } else if (target_type == type_uid<int8_t>()) {
        int32_t v;
        if (JS_ToInt32(ctx, &v, val) == 0) {
            return make_any(static_cast<int8_t>(v));
        }
    } else if (target_type == type_uid<uint8_t>()) {
        uint32_t v;
        if (JS_ToUint32(ctx, &v, val) == 0) {
            return make_any(static_cast<uint8_t>(v));
        }
    } else if (target_type == type_uid<int16_t>()) {
        int32_t v;
        if (JS_ToInt32(ctx, &v, val) == 0) {
            return make_any(static_cast<int16_t>(v));
        }
    } else if (target_type == type_uid<uint16_t>()) {
        uint32_t v;
        if (JS_ToUint32(ctx, &v, val) == 0) {
            return make_any(static_cast<uint16_t>(v));
        }
    } else if (target_type == type_uid<bool>()) {
        int v = JS_ToBool(ctx, val);
        if (v >= 0) {
            return make_any(v != 0);
        }
    } else if (target_type == type_uid<string>()) {
        size_t len = 0;
        const char* str = JS_ToCStringLen(ctx, &len, val);
        if (str) {
            auto result = make_string_any(str, len);
            JS_FreeCString(ctx, str);
            return result;
        }
    }
    return nullptr;
}

} // namespace velk
