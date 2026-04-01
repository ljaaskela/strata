#ifndef VELK_JS_ANY_H
#define VELK_JS_ANY_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_any.h>

extern "C" {
#include <quickjs.h>
}

namespace velk {

/**
 * @brief Interface for JS-backed IAny values that need lifecycle management.
 *
 * Called to release the held JSValue before the JSRuntime is freed.
 * Chain: IInterface -> IJsAny
 */
class IJsAny : public Interface<IJsAny>
{
public:
    virtual void set(JSContext* ctx, JSValue val) = 0;
    virtual void invalidate() = 0;
    virtual JSValue js_value() const = 0;
};

/**
 * @brief IAny implementation that holds a JS value and supports type conversions.
 *
 * Reports compatibility with all numeric types, bool, and string.
 * get_data performs the conversion from the internal JS value to the
 * requested type on demand.
 */
class JsAny final : public ext::ObjectCore<JsAny, IAny, IJsAny>
{
public:
    VELK_CLASS_UID("f1a2b3c4-d5e6-7890-abcd-ef0123456789", "JsAny");

    JsAny() = default;
    ~JsAny() override;

    // IJsAny
    void set(JSContext* ctx, JSValue val) override;
    void invalidate() override;
    JSValue js_value() const override { return val_; }

    // IObject
    Uid get_class_uid() const override { return static_class_id(); }
    string_view get_class_name() const override { return static_class_name(); }
    IObject::Ptr get_self() const override { return {}; }
    uint32_t get_object_flags() const override { return 0; }
    string get_name() const override { return {}; }

    // IAny
    array_view<Uid> get_compatible_types() const override;
    size_t get_data_size(Uid type) const override;
    ReturnValue get_data(void* to, size_t toSize, Uid type) const override;
    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override;
    ReturnValue copy_from(const IAny& other) override;
    IAny::Ptr clone() const override;

private:
    JSContext* ctx_{};
    JSValue val_ = JS_UNDEFINED;
};

} // namespace velk

#endif // VELK_JS_ANY_H
