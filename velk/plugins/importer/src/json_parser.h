#ifndef VELK_IMPORTER_JSON_PARSER_H
#define VELK_IMPORTER_JSON_PARSER_H

#include <velk/string.h>
#include <velk/vector.h>

#include <cstddef>
#include <utility>

namespace velk {

enum class JsonType { Null, Bool, Number, String, Array, Object };

class JsonValue
{
public:
    JsonValue() : type_(JsonType::Null), number_(0.0), bool_(false) {}

    static JsonValue null() { return {}; }
    static JsonValue boolean(bool v)
    {
        JsonValue j;
        j.type_ = JsonType::Bool;
        j.bool_ = v;
        return j;
    }
    static JsonValue number(double v)
    {
        JsonValue j;
        j.type_ = JsonType::Number;
        j.number_ = v;
        return j;
    }
    static JsonValue string(::velk::string v)
    {
        JsonValue j;
        j.type_ = JsonType::String;
        j.string_ = std::move(v);
        return j;
    }
    static JsonValue array(::velk::vector<JsonValue> v)
    {
        JsonValue j;
        j.type_ = JsonType::Array;
        j.array_ = std::move(v);
        return j;
    }
    static JsonValue object(::velk::vector<std::pair<::velk::string, JsonValue>> v)
    {
        JsonValue j;
        j.type_ = JsonType::Object;
        j.object_ = std::move(v);
        return j;
    }

    JsonType type() const { return type_; }

    const ::velk::string& as_string() const { return string_; }
    double as_number() const { return number_; }
    bool as_bool() const { return bool_; }
    const ::velk::vector<JsonValue>& as_array() const { return array_; }
    const ::velk::vector<std::pair<::velk::string, JsonValue>>& as_object() const { return object_; }

    const JsonValue* find(string_view key) const
    {
        if (type_ != JsonType::Object) {
            return nullptr;
        }
        for (auto& pair : object_) {
            if (string_view(pair.first) == key) {
                return &pair.second;
            }
        }
        return nullptr;
    }

    const JsonValue& operator[](string_view key) const
    {
        auto* v = find(key);
        if (!v) {
            static const JsonValue null_value;
            return null_value;
        }
        return *v;
    }

    const JsonValue& operator[](size_t index) const
    {
        if (type_ == JsonType::Array && index < array_.size()) {
            return array_[index];
        }
        static const JsonValue null_value;
        return null_value;
    }

private:
    JsonType type_;
    double number_;
    bool bool_;
    ::velk::string string_;
    ::velk::vector<JsonValue> array_;
    ::velk::vector<std::pair<::velk::string, JsonValue>> object_;
};

/** @brief Parses a JSON string into a JsonValue tree.
 *  @param input  Pointer to the JSON text.
 *  @param length Length in bytes.
 *  @param out    Receives the parsed value on success.
 *  @param error  Receives a human-readable error message on failure.
 *  @return true on success, false on parse error.
 */
bool json_parse(const char* input, size_t length, JsonValue& out, ::velk::string& error);

} // namespace velk

#endif // VELK_IMPORTER_JSON_PARSER_H
