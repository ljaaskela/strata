#ifndef VELK_IMPORTER_JSON_PARSER_H
#define VELK_IMPORTER_JSON_PARSER_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

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
    static JsonValue string(std::string v)
    {
        JsonValue j;
        j.type_ = JsonType::String;
        j.string_ = std::move(v);
        return j;
    }
    static JsonValue array(std::vector<JsonValue> v)
    {
        JsonValue j;
        j.type_ = JsonType::Array;
        j.array_ = std::move(v);
        return j;
    }
    static JsonValue object(std::vector<std::pair<std::string, JsonValue>> v)
    {
        JsonValue j;
        j.type_ = JsonType::Object;
        j.object_ = std::move(v);
        return j;
    }

    JsonType type() const { return type_; }

    const std::string& as_string() const { return string_; }
    double as_number() const { return number_; }
    bool as_bool() const { return bool_; }
    const std::vector<JsonValue>& as_array() const { return array_; }
    const std::vector<std::pair<std::string, JsonValue>>& as_object() const { return object_; }

    const JsonValue* find(const std::string& key) const
    {
        if (type_ != JsonType::Object) {
            return nullptr;
        }
        for (auto& pair : object_) {
            if (pair.first == key) {
                return &pair.second;
            }
        }
        return nullptr;
    }

    const JsonValue& operator[](const std::string& key) const
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
    std::string string_;
    std::vector<JsonValue> array_;
    std::vector<std::pair<std::string, JsonValue>> object_;
};

/** @brief Parses a JSON string into a JsonValue tree.
 *  @param input  Pointer to the JSON text.
 *  @param length Length in bytes.
 *  @param out    Receives the parsed value on success.
 *  @param error  Receives a human-readable error message on failure.
 *  @return true on success, false on parse error.
 */
bool json_parse(const char* input, size_t length, JsonValue& out, std::string& error);

} // namespace velk

#endif // VELK_IMPORTER_JSON_PARSER_H
