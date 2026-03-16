#ifndef VELK_IMPORTER_API_IMPORTER_H
#define VELK_IMPORTER_API_IMPORTER_H

#include <velk/api/velk.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/plugins/importer/plugin.h>

namespace velk {

/** @brief Typed wrapper around an IStoreImporter. */
class Importer
{
public:
    Importer() = default;
    explicit Importer(IStoreImporter::Ptr imp) : imp_(std::move(imp)) {}

    /** @brief Returns true if the importer is valid. */
    explicit operator bool() const { return imp_.operator bool(); }

    /** @brief Imports from a source string and returns a populated store with any errors. */
    ImportResult import_from(string_view source) const
    {
        return imp_ ? imp_->import_from(source) : ImportResult{};
    }

    /** @brief Returns the underlying IStoreImporter pointer. */
    IStoreImporter::Ptr get() const { return imp_; }

private:
    IStoreImporter::Ptr imp_;
};

/** @brief Creates a JSON importer. Loads the importer plugin if not already loaded. */
inline Importer create_json_importer()
{
    ::velk::get_or_load_plugin<IImporterPlugin>(PluginId::ImporterPlugin);
    auto obj = ::velk::instance().create<IStoreImporter>(ClassId::JsonImporter);
    return obj ? Importer(obj) : Importer{};
}

/**
 * @brief Registers a class alias on the importer plugin.
 *
 * Aliases apply to all importers. Not needed for normal use; class names
 * are resolved automatically via the type registry.
 */
inline void register_import_alias(string_view alias, Uid class_uid)
{
    auto ip = ::velk::get_or_load_plugin<IImporterPlugin>(PluginId::ImporterPlugin);
    if (ip) {
        ip->register_class_alias(alias, class_uid);
    }
}

} // namespace velk

#endif // VELK_IMPORTER_API_IMPORTER_H
