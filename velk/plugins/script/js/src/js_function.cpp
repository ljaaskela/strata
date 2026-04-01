#include "js_function.h"

#include "js_any.h"

#include <velk/api/velk.h>
#include <velk/common.h>
#include <velk/string.h>

namespace velk {

namespace {

struct JsFunctionContext
{
    JsStoreContext* store_ctx;
    IJsAny::Ptr fn_holder; // Owns the compiled JS function JSValue; invalidated on teardown.
};

JSValue get_fn(JsFunctionContext* ctx)
{
    return ctx->fn_holder ? ctx->fn_holder->js_value() : JS_UNDEFINED;
}

IAny::Ptr js_trampoline(void* ctx_ptr, FnArgs)
{
    auto* ctx = static_cast<JsFunctionContext*>(ctx_ptr);
    if (!ctx->store_ctx) {
        return nullptr;
    }

    JSValue fn = get_fn(ctx);
    if (JS_IsUndefined(fn)) {
        return nullptr;
    }

    JSContext* js_ctx = ctx->store_ctx->ctx();
    JSValue global = JS_GetGlobalObject(js_ctx);
    JSValue result = JS_Call(js_ctx, fn, global, 0, nullptr);
    JS_FreeValue(js_ctx, global);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(js_ctx);
        const char* msg = JS_ToCString(js_ctx, exception);
        VELK_LOG(E, "JS exception: %s", msg ? msg : "(null)");
        if (msg) {
            JS_FreeCString(js_ctx, msg);
        }
        JS_FreeValue(js_ctx, exception);
        return nullptr;
    }

    auto js_any = ext::make_object<JsAny, IJsAny>();
    js_any->set(js_ctx, result);
    ctx->store_ctx->track(js_any);
    return interface_pointer_cast<IAny>(js_any);
}

void js_context_deleter(void* ctx_ptr)
{
    delete static_cast<JsFunctionContext*>(ctx_ptr);
}

} // namespace

IFunction::Ptr create_js_function(JsStoreContext* context, JSValue fn)
{
    // Wrap the compiled JS function in a JsAny for lifecycle management.
    // The JsAny owns the JSValue and will be invalidated on context teardown.
    auto fn_holder = ext::make_object<JsAny, IJsAny>();
    fn_holder->set(context->ctx(), fn);
    context->track(fn_holder);

    auto* ctx = new JsFunctionContext{context, std::move(fn_holder)};
    return ::velk::instance().create_owned_callback(ctx, &js_trampoline, &js_context_deleter);
}

} // namespace velk
