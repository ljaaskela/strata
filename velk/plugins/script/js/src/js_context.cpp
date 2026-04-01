#include "js_context.h"

#include "js_proxy.h"

#include <velk/string.h>

#include <cstring>

namespace velk {

JsStoreContext::JsStoreContext(JSRuntime* rt, IStore& store)
    : ctx_(JS_NewContext(rt))
    , store_(store)
{
}

JsStoreContext::~JsStoreContext()
{
    // Invalidate all tracked JS values so they release their JSValues
    // before the context is destroyed.
    for (auto& weak : tracked_) {
        if (auto p = weak.lock()) {
            p->invalidate();
        }
    }
    tracked_.clear();

    // Free cached proxy values before destroying the context
    for (auto& [_, val] : proxy_cache_) {
        JS_FreeValue(ctx_, val);
    }
    proxy_cache_.clear();
    JS_FreeContext(ctx_);
}

JSValue JsStoreContext::get_or_create_proxy(IObject::Ptr obj)
{
    if (!obj) {
        return JS_UNDEFINED;
    }
    auto* raw = obj.get();
    auto it = proxy_cache_.find(raw);
    if (it != proxy_cache_.end()) {
        return JS_DupValue(ctx_, it->second);
    }
    JSValue proxy = JsProxy::create(ctx_, std::move(obj));
    if (!JS_IsException(proxy)) {
        // Cache a dup so the cache holds a reference
        proxy_cache_[raw] = JS_DupValue(ctx_, proxy);
    }
    return proxy;
}

namespace {

string_view sv(const char* s)
{
    return string_view(s, std::strlen(s));
}

} // namespace

JSValue JsStoreContext::compile_expression(const char* source, const char* filename)
{
    string func_src = "(function() { return (";
    func_src += sv(source);
    func_src += sv("); })");

    return JS_Eval(ctx_, func_src.c_str(), func_src.size(), filename, JS_EVAL_TYPE_GLOBAL);
}

JSValue JsStoreContext::compile_handler(const char* source, const char* filename)
{
    string func_src = "(function() { ";
    func_src += sv(source);
    func_src += sv(" })");

    return JS_Eval(ctx_, func_src.c_str(), func_src.size(), filename, JS_EVAL_TYPE_GLOBAL);
}

void JsStoreContext::track(const IJsAny::Ptr& any)
{
    tracked_.emplace_back(any);
}

void JsStoreContext::register_global(const char* name, IObject::Ptr obj)
{
    JSValue global = JS_GetGlobalObject(ctx_);
    JSValue proxy = get_or_create_proxy(std::move(obj));
    if (!JS_IsException(proxy)) {
        JS_SetPropertyStr(ctx_, global, name, proxy);
    }
    JS_FreeValue(ctx_, global);
}

} // namespace velk
