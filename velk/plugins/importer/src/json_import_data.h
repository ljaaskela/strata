#ifndef VELK_IMPORTER_JSON_IMPORT_DATA_H
#define VELK_IMPORTER_JSON_IMPORT_DATA_H

#include "json_parser.h"

#include <velk/interface/intf_importer_extension.h>

namespace velk {

/**
 * @brief Static null node for the null object pattern.
 *
 * Returns zero/empty for all accessors and returns itself for find/at.
 */
class NullImportData final : public IImportData
{
public:
    Kind kind() const override { return Kind::Null; }
    bool as_bool() const override { return false; }
    double as_number() const override { return 0.0; }
    string_view as_string() const override { return {}; }
    size_t count() const override { return 0; }
    const IImportData* at(size_t) const override { return &instance(); }
    const IImportData* find(string_view) const override { return &instance(); }
    string_view key_at(size_t) const override { return {}; }

    static const NullImportData& instance()
    {
        static const NullImportData null;
        return null;
    }
};

/**
 * @brief Thin adapter wrapping a const JsonValue& as IImportData.
 */
class JsonImportData final : public IImportData
{
public:
    explicit JsonImportData(const JsonValue& value) : value_(value) {}

    Kind kind() const override
    {
        switch (value_.type()) {
        case JsonType::Null: return Kind::Null;
        case JsonType::Bool: return Kind::Bool;
        case JsonType::Number: return Kind::Number;
        case JsonType::String: return Kind::String;
        case JsonType::Array: return Kind::Array;
        case JsonType::Object: return Kind::Object;
        }
        return Kind::Null;
    }

    bool as_bool() const override { return value_.as_bool(); }
    double as_number() const override { return value_.as_number(); }

    string_view as_string() const override
    {
        auto& s = value_.as_string();
        return string_view(s.c_str(), s.size());
    }

    size_t count() const override
    {
        if (value_.type() == JsonType::Array) {
            return value_.as_array().size();
        }
        if (value_.type() == JsonType::Object) {
            return value_.as_object().size();
        }
        return 0;
    }

    const IImportData* at(size_t index) const override
    {
        if (value_.type() == JsonType::Array) {
            auto& arr = value_.as_array();
            if (index < arr.size()) {
                return get_or_create(index, arr[index]);
            }
        } else if (value_.type() == JsonType::Object) {
            auto& obj = value_.as_object();
            if (index < obj.size()) {
                return get_or_create(index, obj[index].second);
            }
        }
        return &NullImportData::instance();
    }

    const IImportData* find(string_view key) const override
    {
        if (value_.type() != JsonType::Object) {
            return &NullImportData::instance();
        }
        auto& obj = value_.as_object();
        for (size_t i = 0; i < obj.size(); i++) {
            if (string_view(obj[i].first.c_str(), obj[i].first.size()) == key) {
                return get_or_create(i, obj[i].second);
            }
        }
        return &NullImportData::instance();
    }

    string_view key_at(size_t index) const override
    {
        if (value_.type() == JsonType::Object) {
            auto& obj = value_.as_object();
            if (index < obj.size()) {
                return string_view(obj[index].first.c_str(), obj[index].first.size());
            }
        }
        return {};
    }

private:
    const IImportData* get_or_create(size_t index, const JsonValue& val) const
    {
        if (index >= children_.size()) {
            children_.resize(index + 1);
        }
        if (!children_[index]) {
            children_[index] = std::make_unique<JsonImportData>(val);
        }
        return children_[index].get();
    }

    const JsonValue& value_;
    mutable std::vector<std::unique_ptr<JsonImportData>> children_;
};

} // namespace velk

#endif // VELK_IMPORTER_JSON_IMPORT_DATA_H
