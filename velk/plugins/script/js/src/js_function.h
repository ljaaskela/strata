#ifndef VELK_JS_FUNCTION_H
#define VELK_JS_FUNCTION_H

#include "js_context.h"

#include <velk/interface/intf_function.h>

extern "C" {
#include <quickjs.h>
}

namespace velk {

/**
 * @brief Creates a Velk IFunction that evaluates a compiled JS function.
 *
 * Uses the owned callback mechanism (create_owned_callback) rather than
 * a custom ObjectCore subclass, so no IStorageOwned overhead.
 *
 * @param context The JS store context (must outlive the returned function).
 * @param fn A compiled JS function value. Ownership is transferred to the callback.
 * @return An IFunction::Ptr, or nullptr on failure.
 */
IFunction::Ptr create_js_function(JsStoreContext* context, JSValue fn);

} // namespace velk

#endif // VELK_JS_FUNCTION_H
