#ifndef VELK_JS_MARSHAL_H
#define VELK_JS_MARSHAL_H

#include <velk/interface/intf_any.h>

extern "C" {
#include <quickjs.h>
}

namespace velk {

/**
 * @brief Converts a Velk IAny value to a QuickJS JSValue.
 * @param ctx The JS context.
 * @param any The Velk value (may be null).
 * @return A JSValue (caller owns; must JS_FreeValue when done). Returns JS_UNDEFINED for null.
 */
JSValue any_to_jsvalue(JSContext* ctx, const IAny* any);

/**
 * @brief Converts a QuickJS JSValue to a Velk IAny, matching the expected type.
 * @param ctx The JS context.
 * @param val The JS value to convert.
 * @param type_uid The expected Velk type UID.
 * @return An owned IAny::Ptr, or nullptr on type mismatch.
 */
IAny::Ptr jsvalue_to_any(JSContext* ctx, JSValue val, Uid type_uid);

} // namespace velk

#endif // VELK_JS_MARSHAL_H
