#ifndef VELK_IMPORTER_JSON_IMPORTER_H
#define VELK_IMPORTER_JSON_IMPORTER_H

#include "json_parser.h"

#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/interface/member_desc.h>
#include <velk/interface/types.h>

#include <string>
#include <unordered_map>

namespace velk {

class JsonImporter
{
public:
    void register_class_alias(string_view import_name, Uid class_uid);
    ImportResult import_from_json(string_view json) const;

private:
    Uid resolve_class(const std::string& class_str, ImportResult& result) const;
    IObject::Ptr create_object(const JsonValue& obj_node, ImportResult& result) const;
    void set_properties(IObject* obj, const JsonValue& props, const ClassInfo& info,
                        ImportResult& result) const;
    void set_property_value(IPropertyInternal* pi, const PropertyKind& pk, const JsonValue& val) const;
    void build_hierarchies(const JsonValue& hierarchies, IStore* store, ImportResult& result) const;

    std::unordered_map<std::string, Uid> aliases_;
};

} // namespace velk

#endif // VELK_IMPORTER_JSON_IMPORTER_H
